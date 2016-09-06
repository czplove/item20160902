// *******************************************************************
// * rules-engine.c
// *
// *
// * Copyright 2014 by Silicon Labs. All rights reserved.         *80*
// *******************************************************************

#include "../../include/af.h"
#include "../../util/common.h"
#include "rules-engine.h"
#include "../device-table/device-table.h"

//------------------------------------------------------------------------------
BindEntry bindTable[BIND_TABLE_SIZE];
BulbEntry bulbTable[BIND_TABLE_SIZE];

void localSendCommand(uint16_t addressTableEntry);
uint16_t rulesEngineFindBind(uint16_t inAddressTableIndex, uint8_t endpoint);

extern void zclBufferSetup(uint8_t frameType, uint16_t clusterId, uint8_t commandId);
extern void zclBufferAddByte(uint8_t byte);
extern void zclBufferAddWord(uint16_t word);

extern void emAfApsFrameEndpointSetup(uint8_t srcEndpoint,
                                      uint8_t dstEndpoint);
extern bool zclCmdIsBuilt;

//------------------------------------------------------------------------------
// forward declarations
void emberAfPluginRulesEngineInitCallback(void);
//void emberAfPluginRulesEngineClearCallback(void);
void initBindEntryPointer( void );
uint16_t findEmptyBindEntry( void );
void initBulbEntries( void );
uint16_t findBulbEntry( uint16_t deviceEntry );
uint16_t findEmptyBulbEntry( void );

static uint8_t currentOutgoingEndpoint;

void emberAfRulesEngineReportAttributeRespones(EmberAfClusterId clusterId,
                                               uint8_t * buffer,
                                               uint16_t bufLen)
{
  uint16_t i;
  uint16_t attributeId;
  uint8_t *attributeData;
  uint16_t inAddressTableEntry, outAddressTableEntry;
  EmberNodeId sourceNode;
  uint8_t inEndpoint;

  attributeId = (uint16_t)buffer[ATTRIBUTE_BUFFER_ID_OFFSET_L] | 
                ((uint16_t)buffer[ATTRIBUTE_BUFFER_ID_OFFSET_U] << 8);
  attributeData = buffer + ATTRIBUTE_BUFFER_DATA_OFFSET;

  sourceNode = emberGetSender();

  emberAfPluginDeviceTableMessageReceived(sourceNode);

  inAddressTableEntry = getAddressIndexFromNodeId(sourceNode);
  inEndpoint = 1; //How do we actually find the endpoint???

  initBindEntryPointer();

  while ((outAddressTableEntry = rulesEngineFindBind(inAddressTableEntry,
                                                     inEndpoint))
         != NULL_ENTRY) {

    // We found a match in the bind table for the incoming command.  Print out 
    // the match and send the outgoing command.

    switch (clusterId) {
    case ZCL_OCCUPANCY_SENSING_CLUSTER_ID:
      if (attributeId == ZCL_OCCUPANCY_ATTRIBUTE_ID) {
        if (*attributeData) {
          emberAfFillCommandOnOffClusterOn();
        } else {
          emberAfFillCommandOnOffClusterOff();
        }
        zclCmdIsBuilt = true;
      }
      break;
    default:
      zclCmdIsBuilt = false;
      break;
    }

    if (zclCmdIsBuilt) {
      // send the command
      localSendCommand(outAddressTableEntry);
    }
  }
}

//------------------------------------------------------------------------------
bool emberAfPreCommandReceivedCallback(EmberAfClusterCommand* cmd)
{
  uint16_t inAddressTableEntry, outAddressTableEntry;
  uint16_t bulbEntry;
  uint8_t hueStep,hueDirection,newHue;
  uint8_t stepMode, stepSize;
  uint16_t transitionTime;
  uint16_t tempStep, newTemp;
  uint8_t tempDirection;
  uint8_t state;
  uint8_t inEndpoint;
  
  // callback for rules engine command received.
  /*emberAfPluginRulesEngineRulesPreCommandReceivedCallback(cmd->commandId,
                                                          cmd->clusterSpecific,
                                                          cmd->apsFrame->clusterId,
                                                          cmd->mfgSpecific,
                                                          cmd->mfgCode,
                                                          cmd->buffer,
                                                          cmd->bufLen,
                                                          cmd->payloadStartIndex);*/

  emberAfPluginDeviceTableMessageReceived(cmd->source);

  inAddressTableEntry = getAddressIndexFromNodeId(cmd->source);
  inEndpoint = cmd->apsFrame->sourceEndpoint;

  emberSerialPrintf(APP_SERIAL, "Entry:  %x:%x\r\n");

  initBindEntryPointer();

  // Code to send MQTT based on switch command
  emberAfPluginRulesEnginePreCommandCallback(cmd);

  while ((outAddressTableEntry = rulesEngineFindBind(inAddressTableEntry, 
                                                     inEndpoint)) 
         != NULL_ENTRY ) {

    // We found a match in the bind table for the incoming command.  Print out 
    // the match and send the outgoing command.  

    emberSerialPrintf(APP_SERIAL, "In:  0x%2x, Out: 0x%2x\r\n", 
                      inAddressTableEntry, outAddressTableEntry);

    switch (cmd->apsFrame->clusterId) {
    case ZCL_ON_OFF_CLUSTER_ID:
      if (cmd->commandId < 0x03) {
        if(cmd->commandId == 0x00) {
          emberAfFillCommandOnOffClusterOff();
        } else {
          emberAfFillCommandOnOffClusterOn();
        }
        zclCmdIsBuilt = true;
      } 
      break;
    case ZCL_IAS_ZONE_CLUSTER_ID:
      state = cmd->buffer[cmd->payloadStartIndex] & 0x01;

      // 0 = closed from sensor, 0 = off for light
      // if closed, turn on light
      if (state == 0) {
        emberAfFillCommandOnOffClusterOn();
      } else {
        emberAfFillCommandOnOffClusterOff();
      }

      zclCmdIsBuilt = true;
      break;
#if 1
    case ZCL_LEVEL_CONTROL_CLUSTER_ID:
      stepMode = cmd->buffer[cmd->payloadStartIndex + 0];
      stepSize = cmd->buffer[cmd->payloadStartIndex + 1];
      transitionTime = cmd->buffer[cmd->payloadStartIndex + 2];

      emberAfFillCommandLevelControlClusterStepWithOnOff(stepMode, stepSize, transitionTime);
      zclCmdIsBuilt = true;
      break;

    case ZCL_COLOR_CONTROL_CLUSTER_ID:
      // first, find the 
      bulbEntry = findBulbEntry(outAddressTableEntry);
      // assert(bulbEntry != NULL_ENTRY);
      if (bulbEntry == NULL_ENTRY) {
        emberSerialPrintf(APP_SERIAL, "Rules engine: did not find bulb entry for: %d", outAddressTableEntry); 
        return false;
      }
      switch (cmd->commandId) {
      case ZCL_STEP_HUE_COMMAND_ID:
        hueStep = cmd->buffer[cmd->payloadStartIndex + 1];
        hueDirection = cmd->buffer[cmd->payloadStartIndex + 0];
        if (hueDirection == 1) {
          newHue = bulbTable[bulbEntry].currentHue + hueStep;
          if (newHue < bulbTable[bulbEntry].currentHue)
            newHue = 0xFE;
        }
        if (hueDirection == 3) {
          newHue = bulbTable[bulbEntry].currentHue - hueStep;
          if (newHue > bulbTable[bulbEntry].currentHue)
            newHue = 0;
        }
        bulbTable[bulbEntry].currentHue = newHue;

        emberAfFillCommandColorControlClusterMoveToHueAndSaturation(newHue, 254, 0);
        zclCmdIsBuilt = true;
        break;
      case ZCL_STEP_COLOR_COMMAND_ID:
        // ignore for now
        break;
      case ZCL_STEP_COLOR_TEMPERATUE_COMMAND_ID:
        tempStep = HIGH_LOW_TO_INT(cmd->buffer[cmd->payloadStartIndex + 2],
                                   cmd->buffer[cmd->payloadStartIndex + 1]);
        tempDirection = cmd->buffer[cmd->payloadStartIndex + 0];
        emberSerialPrintf(APP_SERIAL, "\r\nRules. Step Command: TempStep: %d , TempDir: %d \r\n", 
                      tempStep, tempDirection);
        if (tempDirection == 1) {
          newTemp = bulbTable[bulbEntry].currentMiredTemp + tempStep;
          if (newTemp > 377) {
            newTemp = 377;
          }
        }
        if (tempDirection == 3) {
          newTemp = bulbTable[bulbEntry].currentMiredTemp - tempStep;
          if (newTemp > 377 || newTemp < 150) {
            newTemp = 150;
          }
        }
        bulbTable[bulbEntry].currentMiredTemp = newTemp;

        emberAfFillCommandColorControlClusterMoveToColorTemperature(newTemp, 0);
        zclCmdIsBuilt = true;
        break;
      case ZCL_MOVE_TO_HUE_COMMAND_ID:
        bulbTable[bulbEntry].currentHue = cmd->buffer[cmd->payloadStartIndex];
        break;
      case ZCL_MOVE_TO_HUE_AND_SATURATION_COMMAND_ID:
        bulbTable[bulbEntry].currentHue = cmd->buffer[cmd->payloadStartIndex];
        break;
      case ZCL_MOVE_TO_COLOR_COMMAND_ID:
        // ignore for now
        break;
      case ZCL_MOVE_TO_COLOR_TEMPERATURE_COMMAND_ID:
        emberSerialPrintf(APP_SERIAL, "\nRules: NewTempCommand: %d \r\n", 
                                    cmd->buffer[cmd->payloadStartIndex]);
        bulbTable[bulbEntry].currentMiredTemp = cmd->buffer[cmd->payloadStartIndex];
        break;
      }

      if (cmd->commandId == ZCL_STEP_HUE_COMMAND_ID ||
         cmd->commandId == ZCL_STEP_COLOR_TEMPERATUE_COMMAND_ID) {
        break;
      }
      // We did not yet build the command.  We can pass it on as is.  
#endif
  default:
      emberSerialPrintf(APP_SERIAL, "\nDefault case rules-engine\r\n");
      zclCmdIsBuilt = false;
      break;
    }

    if (zclCmdIsBuilt) {
      // send the command
      localSendCommand(outAddressTableEntry);
    }
  }

  // process the actions rules engine.
  processActionsCmd(cmd, inEndpoint);

  // Even if we got a hit here, we still want the framework to process
  // the command.
  return false;
}

extern EmberApsFrame globalApsFrame;
extern uint8_t appZclBuffer[];
extern uint16_t appZclBufferLen;
extern uint16_t mfgSpecificId;

void localSendCommand(uint16_t addressTableEntry)
{
  if (currentOutgoingEndpoint == UNDEFINED_ENDPOINT) {
    deviceTableSend(addressTableEntry);
  } else {
    deviceTableSendWithEndpoint(addressTableEntry, currentOutgoingEndpoint);
  }
}

void addBindEntryEndpoint(uint16_t indexIn, 
                          uint8_t inEndpoint, 
                          uint16_t indexOut, 
                          uint8_t outEndpoint) 
{
  uint16_t entry = findEmptyBindEntry();
  uint16_t bulbEntry = findEmptyBulbEntry();  // track possible bulb commands

  // work on entry 0 for now...
  bindTable[entry].inAddressTableEntry = indexIn;
  bindTable[entry].inEndpoint = inEndpoint;
  bindTable[entry].outAddressTableEntry = indexOut;
  bindTable[entry].outEndpoint = outEndpoint;

  bulbTable[bulbEntry].deviceEntry = indexOut;

  emberSerialPrintf(APP_SERIAL, "bind:  0x%2x 0x%2x\r\n", indexIn, indexOut);

  // Save on new Bind Entry command
  rulesEngineBindSaveCommand();
  rulesEngineSaveCommand();
  emberAfPluginRulesEngineChangedCallback();
}

void addBindEntry(uint16_t indexIn, uint16_t indexOut)
{
  addBindEntryEndpoint(indexIn, UNDEFINED_ENDPOINT, indexOut, UNDEFINED_ENDPOINT);
}

void initBindTable( void )
{
  uint16_t i;

  for (i=0; i<BIND_TABLE_SIZE; i++) {
    bindTable[i].inAddressTableEntry = NULL_ENTRY;
    bindTable[i].outAddressTableEntry = NULL_ENTRY;
  }
}

void emberAfPluginRulesEngineInitCallback(void)
{
  initBindTable();
  emAfPluginRulesEngineInitActions();
  initBulbEntries();

  // Load the rules on init
  rulesEngineBindLoadCommand();
  rulesEngineLoadCommand();

  // Publish a change to esatablish the new rules engine
  //emberAfPluginRulesEngineChangedCallback();
}

uint16_t findEmptyBindEntry( void )
{
  uint16_t i;

  for (i=0; i< BIND_TABLE_SIZE; i++) {
    if (bindTable[i].outAddressTableEntry == NULL_ENTRY)
      return i;
  }
  return NULL_ENTRY;
}

static uint16_t bindEntryPointer;
void initBindEntryPointer( void )
{
  bindEntryPointer = NULL_ENTRY;
}

uint16_t rulesEngineFindBind(uint16_t inAddressTableEntry, uint8_t inEndpoint)
{
  for (bindEntryPointer++ ; bindEntryPointer< BIND_TABLE_SIZE; bindEntryPointer++) {

    if ( (bindTable[bindEntryPointer].inAddressTableEntry == inAddressTableEntry)
          && ( (bindTable[bindEntryPointer].inEndpoint == UNDEFINED_ENDPOINT)
               || (bindTable[bindEntryPointer].inEndpoint == inEndpoint) ) ) {
      currentOutgoingEndpoint = bindTable[bindEntryPointer].outEndpoint;
      return bindTable[bindEntryPointer].outAddressTableEntry;
    }
  }
  return NULL_ENTRY;
}

// ************************************
// Storing bulb state on the gateway.
// 
// The original RGB bulb reference design did not handle step color, step hue, 
// or step temperature commands.  So we needed to intercept these commands at 
// the gateway and translate them to the moveTo command variety.
void initBulbEntries( void )
{
  uint16_t i;

  for (i=0; i<BIND_TABLE_SIZE; i++) {
    bulbTable[i].deviceEntry = NULL_ENTRY;
    bulbTable[i].currentMiredTemp = 150;
    bulbTable[i].currentHue = 0;
  }
}

uint16_t findBulbEntry( uint16_t deviceEntry )
{
  uint16_t i;

  for (i=0; i<BIND_TABLE_SIZE; i++) {
    if (bulbTable[i].deviceEntry == deviceEntry) {
      return i;
    }
  }

  return NULL_ENTRY;
}

uint16_t findEmptyBulbEntry( void ) {
  uint16_t i;

  for (i=0; i<BIND_TABLE_SIZE; i++) {
    if (bulbTable[i].deviceEntry == NULL_ENTRY) {
      return i;
    }
  }

  return NULL_ENTRY;
}


