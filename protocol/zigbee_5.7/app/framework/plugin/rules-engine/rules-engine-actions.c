// *******************************************************************
// * rules-engine.c
// *
// *
// * Copyright 2014 by Silicon Labs. All rights reserved.         *80*
// *******************************************************************

#include "../../include/af.h"
#include "../../util/common.h"
#include "rules-engine.h"

//------------------------------------------------------------------------------
ActionEntry actionTable[ACTION_TABLE_SIZE];

void localSendCommand(EmberNodeId destination);

extern void zclBufferSetup(uint8_t frameType, uint16_t clusterId, uint8_t commandId);
extern void zclBufferAddByte(uint8_t byte);
extern void zclBufferAddWord(uint16_t word);

extern void emAfApsFrameEndpointSetup(uint8_t srcEndpoint,
                                      uint8_t dstEndpoint);
extern bool zclCmdIsBuilt;

extern void addressTableSend(uint16_t index);
extern uint16_t getAddressIndexFromNodeId(EmberNodeId emberNodeId);

static uint16_t ruleKill = 0;
static uint8_t outgoingEndpoint;

//------------------------------------------------------------------------------
// forward declarations
EmberNodeId rulesEngineFindAction(EmberNodeId in);

//------------------------------------------------------------------------------
void sendAction(ActionEntry actionEntry)
{
  switch (actionEntry.outAction) {
  case OUT_ACTION_OFF:
    zclSimpleClientCommand(ZCL_ON_OFF_CLUSTER_ID, 
                           ZCL_OFF_COMMAND_ID);
    zclCmdIsBuilt = true;
    break;
  case OUT_ACTION_ON:
    zclSimpleClientCommand(ZCL_ON_OFF_CLUSTER_ID, 
                           ZCL_ON_COMMAND_ID);
    zclCmdIsBuilt = true;
    break;
  case OUT_ACTION_LEVEL:
    zclBufferSetup(ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER, 
                   ZCL_LEVEL_CONTROL_CLUSTER_ID, 
                   ZCL_MOVE_TO_LEVEL_WITH_ON_OFF_COMMAND_ID);
    zclBufferAddByte( actionEntry.outActionData.level );
    zclBufferAddByte( 0 );
    zclBufferAddByte( 0 );

    zclCmdIsBuilt = true;
    break;
  case OUT_ACTION_LOCK:
    zclSimpleClientCommand(ZCL_DOOR_LOCK_CLUSTER_ID,
                           ZCL_LOCK_DOOR_COMMAND_ID);
    zclCmdIsBuilt = true;
    break;
  case OUT_ACTION_UNLOCK:
    zclSimpleClientCommand(ZCL_DOOR_LOCK_CLUSTER_ID,
                           ZCL_UNLOCK_DOOR_COMMAND_ID);
    zclCmdIsBuilt = true;
    break;
  case OUT_ACTION_COLOR_XY:
    zclBufferSetup(ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER, 
                   ZCL_COLOR_CONTROL_CLUSTER_ID, 
                   ZCL_MOVE_TO_COLOR_COMMAND_ID);
    zclBufferAddWord(actionEntry.outActionData.colorXy.colorX);
    zclBufferAddWord(actionEntry.outActionData.colorXy.colorY);
    zclBufferAddWord(0);

    zclCmdIsBuilt = true;
  case OUT_ACTION_PARTY:
    srand (time(NULL));

    uint16_t color = (uint16_t)
      (rand() % 7);
    color = color * 2;
    color = color + 2;
    zclBufferSetup(ZCL_CLUSTER_SPECIFIC_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER, 
                   ZCL_COLOR_CONTROL_CLUSTER_ID, 
                   ZCL_MOVE_TO_COLOR_COMMAND_ID);
    zclBufferAddWord(0);
    zclBufferAddWord(color);
    zclBufferAddWord(0);

    emberSerialPrintf(APP_SERIAL, "%d\r\n", color);

    zclCmdIsBuilt = true;
    break;
  default:
    zclCmdIsBuilt = false;
    break;
  }
  if (zclCmdIsBuilt) {
    if (outgoingEndpoint == UNDEFINED_ENDPOINT) {
      deviceTableSend(actionEntry.outAddressTableEntry);
    } else {
      deviceTableSendWithEndpoint(actionEntry.outAddressTableEntry, outgoingEndpoint);
    }

    zclCmdIsBuilt = false;
  }
}

uint8_t grabLocalEndpoint( void )
{
  uint8_t fixedEndpoints[] = FIXED_ENDPOINT_ARRAY;

  return fixedEndpoints[0];
}

uint8_t mapAction(EmberAfClusterCommand* cmd)
{
  uint16_t clusterId = cmd->apsFrame->clusterId;
  uint8_t  commandId = cmd->commandId;

  if (cmd->clusterSpecific == false) {
    return IN_ACTION_NONE;
  }

  emberSerialPrintf(APP_SERIAL, "In Action Cluster: %2x, Command %x\r\n", clusterId, commandId);

  switch(clusterId) {
  case ZCL_ON_OFF_CLUSTER_ID:
    switch(commandId) {
    case ZCL_OFF_COMMAND_ID:
      return IN_ACTION_OFF;
      break;
    case ZCL_ON_COMMAND_ID:
      return IN_ACTION_ON;
      break;
    }
    break;
  case ZCL_LEVEL_CONTROL_CLUSTER_ID:
    emberSerialPrintf(APP_SERIAL, "Unmapped InAction Level\r\n");
    break;
  case ZCL_IAS_ZONE_CLUSTER_ID:
    if (commandId == ZCL_ALARM_COMMAND_ID) {
      uint8_t alarmStatus = cmd->buffer[ cmd->payloadStartIndex ];

      if ( (alarmStatus & BIT(0)) == BIT(0) ) {
        return IN_ACTION_ALARM_ON;
      } else {
        return IN_ACTION_ALARM_OFF;
      }
    }
    break;
  default:
    emberSerialPrintf(APP_SERIAL, "Unmapped InAction\r\n");
    break;
  }

  return IN_ACTION_NONE;
}

bool inActionMatch(ActionEntry actionEntry, 
                      uint8_t inAction, 
                      uint16_t inAddressTableEntry, 
                      uint8_t inEndpoint)
{
  if (actionEntry.inAction == inAction
    && actionEntry.inAddressTableEntry == inAddressTableEntry
    && (actionEntry.inEndpoint == 0xff
        || actionEntry.inEndpoint == inEndpoint) ) {
    outgoingEndpoint = actionEntry.outEndpoint;
    return true;
  } else {
    return false;
  }
}

void processActionsCmd(EmberAfClusterCommand* cmd, uint8_t inEndpoint)
{
  uint16_t i;
  uint8_t inAction = mapAction(cmd);
  uint16_t inAddressTableEntry = getAddressIndexFromNodeId( cmd->source );

  if (inAction == IN_ACTION_NONE) {
    return;
  }

  // we have a number of rules we wish to kill.  
  if (ruleKill > 0) {
    ruleKill--;
    return;
  }

  for(i=0; i<ACTION_TABLE_SIZE; i++)
  {
    if (inActionMatch(actionTable[i], inAction, inAddressTableEntry, inEndpoint)) {
      emberSerialPrintf(APP_SERIAL, "Sending Action %2x\r\n", i);
      sendAction(actionTable[i]);
    }
  }
}

uint16_t findBlankAction( void )
{
  uint16_t i;

  for(i=0; i<ACTION_TABLE_SIZE; i++) {
    if (actionTable[i].inAction == 0xff) {
      return i;
    }
  }

  return 0xffff;
}

void emAfPluginRulesEngineInitActions( void )
{
  uint16_t i;
  for(i=0; i<ACTION_TABLE_SIZE; i++) {
    actionTable[i].inAction = 0xff;
    actionTable[i].inAddressTableEntry = 0xffff;
  }
}

void rulesEngineClearCommand( void )
{
  emAfPluginRulesEngineInitActions();
}

void emberAfPluginRulesEngineKillRule(void)
{
  ruleKill++;
}