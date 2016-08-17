// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#include PLATFORM_HEADER

#ifdef EZSP_HOST
  // Includes needed for ember related functions for the EZSP host
  #include "stack/include/error.h"
  #include "stack/include/ember-types.h"
  #include "app/util/ezsp/ezsp-protocol.h"
  #include "app/util/ezsp/ezsp.h"
  #include "app/util/ezsp/serial-interface.h"
#else
  #include "stack/include/ember.h"
#endif

#include "app/framework/include/af.h"
#include "app/framework/util/tokens.h"
#include "hal/hal.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/include/af.h"
#include "app/util/source-route-common.h"
#include "app/util/zigbee-framework/zigbee-device-common.h"
#include "app/framework/plugin/device-table/device-table.h"
#include "app/framework/plugin/rules-engine/rules-engine.h"
#include "app/util/security/security.h"

#define emberAfAppPrintln(...) emberAfPrintln(EMBER_AF_PRINT_APP,  __VA_ARGS__)

// Re-discover a node already on the network.  Useful for when I power cycle
// the gateway.
#if defined(__MCCXAP2B__)
  // The XAP is very strict about type conversions with regard to EmberEUI64.
  // In order to cast "const int8u*" to "const EmberEUI64" 
  // we must first cast away the const of the int8u* type and then cast the
  // data to a 'const EmberEUI64'.
  #define INT8U_TO_EUI64_CAST(x) ((const EmberEUI64)((int8u*)x)) 
#else
  #define INT8U_TO_EUI64_CAST(x) (x)
#endif

// the number of tokens that can be written using ezspSetToken and read
// using ezspGetToken
#define MFGSAMP_NUM_EZSP_TOKENS 8
// the size of the tokens that can be written using ezspSetToken and
// read using ezspGetToken
#define MFGSAMP_EZSP_TOKEN_SIZE 8
// the number of manufacturing tokens 
#define MFGSAMP_NUM_EZSP_MFG_TOKENS 11
// the size of the largest EZSP Mfg token, EZSP_MFG_CBKE_DATA
// please refer to app/util/ezsp/ezsp-enum.h
#define MFGSAMP_EZSP_TOKEN_MFG_MAXSIZE 92

static EmberNodeId discoverNode;

extern void newDeviceJoinHandler(EmberNodeId newNodeId,
                                 EmberEUI64 newNodeEui64);
// External declarations
extern AddressTableEntry addressTable[];
void newDeviceJoinHandler(EmberNodeId newNodeId,
                          EmberEUI64 newNodeEui64);
void printChildTable(void);
void zclBufferAddByteFromArgument(uint8_t index);
void zclBufferAddWordFromArgument(uint8_t index);
void cliBufferPrint(void);

void emAfExampleCommand(void);
void emAfPwm1TestCommand(void);
void emAfPwm2TestCommand(void);
void emAfSendLeaveZdoCommand(void);
void emAfLeaveZdoAtCommand(void);
void emAfSendLeaveApsCommand(void);
void emAfPrintTcCloneInfoCommand(void);
void emAfInputTcCloneInfoCommand(void);
void printEndpointCommand(void);
void printSourceRouteTable(void);
void mfgappTokenDump(void);
void configureBindingCommand(void);
void configureReportCommand(void);
void discoverPresentNodeCommand(void);
void addressTableSendCommand(void);
void changeNwkKeyCommand( void );
void printNextKeyCommand( void );
void printChildTableCommand( void );
void killRuleCommand( void );
void versionCommand( void );
static void customPjoinCommand( void );

void setTxPowerCommand( void );

// HA test case commands
void channelChangeCommand( void );
void discoverAttributesCommand( void );
void onOffTestCommand( void );

extern EmberApsFrame globalApsFrame;
extern uint8_t appZclBuffer[];
extern uint16_t appZclBufferLen;
extern boolean zclCmdIsBuilt;

void zclBufferSetup(uint8_t frameType, uint16_t clusterId, uint8_t commandId); 
void zclBufferAddByte(uint8_t byte);
void zclBufferAddWord(uint16_t word);
void zclBufferAddWordFromArgument(uint8_t index);
void emAfApsFrameEndpointSetup(uint8_t srcEndpoint,
                                      uint8_t dstEndpoint);

extern void printAddressTable( void );
extern void clearDeviceTable( void );
extern void emberTrafficTestOnOffCommand( void );
extern EmberStatus getEui64FromNodeId( EmberNodeId emberNodeId, EmberEUI64 eui64);

#define onOffTestEventControl emberAfPluginGatewayCommandsOnOffTestEventControl
EmberEventControl emberAfPluginGatewayCommandsOnOffTestEventControl;

EmberCommandEntry emberAfCustomCommands[] = {
  /* Sample Custom CLI commands */
  emberCommandEntryAction( "example", emAfExampleCommand, "", ""),
  emberCommandEntryAction( "send_leave_zdo", emAfSendLeaveZdoCommand, "vb", ""),
  emberCommandEntryAction( "leave_zdo", emAfLeaveZdoAtCommand, "v", ""),
  emberCommandEntryAction( "send_leave_aps", emAfSendLeaveApsCommand, "vb", ""),
  emberCommandEntryAction( "TC_out", emAfPrintTcCloneInfoCommand, "", ""),
  emberCommandEntryAction( "TC_in", emAfInputTcCloneInfoCommand,"b", ""),
  emberCommandEntryAction( "printat", printAddressTable, "", ""),
  emberCommandEntryAction( "printep", printEndpointCommand, "vu", ""),
  emberCommandEntryAction( "cleardevicetable", clearDeviceTable, "", ""),
  emberCommandEntryAction( "print_srt", printSourceRouteTable, "", ""),
  emberCommandEntryAction( "tokdump", mfgappTokenDump, "", ""),
  emberCommandEntryAction( "binding", configureBindingCommand, "v", ""),
  emberCommandEntryAction( "report", configureReportCommand, "v", ""),
  emberCommandEntryAction( "disc", discoverPresentNodeCommand, "v", ""),
  emberCommandEntryAction( "send", addressTableSendCommand, "u", ""),
  emberCommandEntryAction( "changeNwkKey", changeNwkKeyCommand, "", ""),
  emberCommandEntryAction( "printNextKey", printNextKeyCommand, "", ""),
  emberCommandEntryAction( "printChildTable", printChildTableCommand, "", ""),
  emberCommandEntryAction( "killrule", killRuleCommand, "", ""),
  emberCommandEntryAction( "channelChange", channelChangeCommand, "u", ""),
  emberCommandEntryAction( "discAttributes", discoverAttributesCommand, "vvuu", ""),
  emberCommandEntryAction( "onOffTest", emberTrafficTestOnOffCommand, "vvv", ""),
  emberCommandEntryAction( "version", versionCommand, "", ""),
  emberCommandEntryAction( "txPower", setTxPowerCommand, "s", ""),
  emberCommandEntryAction( "pjoin", customPjoinCommand, "u", ""),
  emberCommandEntryTerminator()
};
//// ******* test of token dump code
static uint8_t serialPort = APP_SERIAL;

// the manufacturing tokens are enumerated in app/util/ezsp/ezsp-protocol.h
// the names are enumerated here to make it easier for the user
PGM_NO_CONST PGM_P ezspMfgTokenNames[] =
  {
    "EZSP_MFG_CUSTOM_VERSION...",
    "EZSP_MFG_STRING...........",
    "EZSP_MFG_BOARD_NAME.......",
    "EZSP_MFG_MANUF_ID.........",
    "EZSP_MFG_PHY_CONFIG.......",
    "EZSP_MFG_BOOTLOAD_AES_KEY.",
    "EZSP_MFG_ASH_CONFIG.......",
    "EZSP_MFG_EZSP_STORAGE.....",
    "EZSP_STACK_CAL_DATA.......",
    "EZSP_MFG_CBKE_DATA........",
    "EZSP_MFG_INSTALLATION_CODE"
  };

extern AddressTableEntry addressTable[];

void emAfPrintTcCloneInfoCommand(void)
{}

void emAfInputTcCloneInfoCommand(void)
{}

void swapEndianEui64(EmberEUI64 eui64)
{
  uint8_t temp, i;

  for (i = 0; i < 4; i++)
  {
    temp = eui64[i];
    eui64[i] = eui64[7-i];
    eui64[7-i] = temp;
  }
}

EmberStatus emberSendRemoveDevice(EmberNodeId destShort,
                                  EmberEUI64 destLong,
                                  EmberEUI64 deviceToRemoveLong);
void emAfSendLeaveCommand(void)
{
  uint16_t nodeId = (uint16_t)emberUnsignedCommandArgument(0);
  EmberEUI64 destinationEui64;

  emberCopyStringArgument(1,
                          destinationEui64,
                          EUI64_SIZE,
                          0);

  // eui64 is stored little endian.  but it is easier to input
  // the eui64 as big endian.  So we need to swap it.
  swapEndianEui64(destinationEui64); 

  // use APS command to remove device
  emberSendRemoveDevice(nodeId, destinationEui64, destinationEui64);
}

void emAfLeaveZdoAtCommand(void)
{
  uint16_t index = (uint16_t)emberUnsignedCommandArgument(0);

  emberAfPluginDeviceTableSendLeave( index );
}

void emAfSendLeaveZdoCommand(void)
{
  uint16_t nodeId = (uint16_t)emberUnsignedCommandArgument(0);
  EmberEUI64 destinationEui64;

  EmberApsOption apsOptions = EMBER_APS_OPTION_RETRY | 
    EMBER_APS_OPTION_ENABLE_ROUTE_DISCOVERY |
    EMBER_APS_OPTION_ENABLE_ADDRESS_DISCOVERY;

  emberCopyStringArgument(1,
                          destinationEui64,
                          EUI64_SIZE,
                          0);

  // eui64 is stored little endian.  but it is easier to input
  // the eui64 as big endian.  So we need to swap it.
  swapEndianEui64(destinationEui64); 

  // use the ZDO command to remove the device
  emberLeaveRequest(nodeId,
                    destinationEui64,
                    0x00,        // Just leave.  Do not remove children, if any.
                    apsOptions);
}

void emAfSendLeaveApsCommand(void)
{
  uint16_t nodeId = (uint16_t)emberUnsignedCommandArgument(0);
  EmberEUI64 destinationEui64;

  emberCopyStringArgument(1,
                          destinationEui64,
                          EUI64_SIZE,
                          0);

  // eui64 is stored little endian.  but it is easier to input
  // the eui64 as big endian.  So we need to swap it.
  swapEndianEui64(destinationEui64); 

  // use APS command to remove device
  emberSendRemoveDevice(nodeId, destinationEui64, destinationEui64);
}

void emAfExampleCommand(void)
{
  emberAfAppPrintln("example command!\n");
}


extern void printEndpoint( uint16_t nodeId, uint8_t endpoint );

// custom printep
// custom print at <2: node ID> <1: endpoint>
void printEndpointCommand(void)
{
  uint16_t nodeId = (uint16_t)emberUnsignedCommandArgument(0);
  uint8_t endpoint = emberUnsignedCommandArgument(1);

  printEndpoint( nodeId, endpoint );
}

//// code to print out the source route table
void printSourceRouteTable( void ) {
  uint8_t i;

  for (i=0 ; i < sourceRouteTableSize ; i++) {
    //emberSerialPrintf(APP_SERIAL, "%2x-%x-%x ", sourceRouteTable[i].destination, sourceRouteTable[i].closerIndex,sourceRouteTable[i].olderIndex);
    if (sourceRouteTable[i].destination != 0x0000) {
      emberSerialPrintf(APP_SERIAL, "[ind]%x[dest]%2x[closer]%x[older]%x\r\n", i,sourceRouteTable[i].destination, sourceRouteTable[i].closerIndex,sourceRouteTable[i].olderIndex);
    }
    emberSerialWaitSend(APP_SERIAL);
  }
  emberSerialPrintf(APP_SERIAL, "<print srt>\r\n\r\n");
  emberSerialWaitSend(APP_SERIAL);
}


// ******************************
// mfgappTokenDump
// ******************************
// called to dump all of the tokens. This dumps the indices, the names, 
// and the values using ezspGetToken and ezspGetMfgToken. The indices 
// are used for read and write functions below.
//
void mfgappTokenDump(void)
{
#ifdef EZSP_HOST

  EmberStatus status;
  uint8_t tokenData[MFGSAMP_EZSP_TOKEN_MFG_MAXSIZE];
  uint8_t index, i, tokenLength;

  // first go through the tokens accessed using ezspGetToken
  emberSerialPrintf(serialPort,"(data shown little endian)\r\n");
  emberSerialPrintf(serialPort,"Tokens:\r\n");
  emberSerialPrintf(serialPort,"idx  value:\r\n");
  for(index=0; index<MFGSAMP_NUM_EZSP_TOKENS; index++) {

    // get the token data here
    status = ezspGetToken(index, tokenData);
    emberSerialPrintf(serialPort,"[%d]", index);
    if (status == EMBER_SUCCESS) {

      // Print out the token data
      for(i = 0; i < MFGSAMP_EZSP_TOKEN_SIZE; i++) {
        emberSerialPrintf(serialPort, " %X", tokenData[i]);
      }

      emberSerialWaitSend(serialPort);
      emberSerialPrintf(serialPort,"\r\n");
    }
    // handle when ezspGetToken returns an error
    else {
      emberSerialPrintf(serialPort," ... error 0x%x ...\r\n",
                        status);
    }
  }

  // now go through the tokens accessed using ezspGetMfgToken
  // the manufacturing tokens are enumerated in app/util/ezsp/ezsp-protocol.h
  // this file contains an array (ezspMfgTokenNames) representing the names.
  emberSerialPrintf(serialPort,"Manufacturing Tokens:\r\n");
  emberSerialPrintf(serialPort,
                    "idx  token name                 len   value\r\n");
  for(index=0; index<MFGSAMP_NUM_EZSP_MFG_TOKENS; index++) {

    // ezspGetMfgToken returns a length, be careful to only access
    // valid token indices.
    tokenLength = ezspGetMfgToken(index, tokenData);
    emberSerialPrintf(serialPort,"[%x] %p: 0x%x:", index,
                      ezspMfgTokenNames[index], tokenLength);

    // Print out the token data
    for(i = 0; i < tokenLength; i++) {
      if ((i != 0) && ((i % 8) == 0)) {
        emberSerialPrintf(serialPort,
                          "\r\n                                    :");
        emberSerialWaitSend(serialPort);
      }
      emberSerialPrintf(serialPort, " %X", tokenData[i]);
    }
    emberSerialWaitSend(serialPort);
    emberSerialPrintf(serialPort,"\r\n");
  }
  emberSerialPrintf(serialPort,"\r\n");
#endif
}

void configureBindingCommand(void) {
  EmberEUI64 sourceEui, destEui;
  uint16_t nodeId = (uint16_t)emberUnsignedCommandArgument(0);
  EmberStatus status;

  if (getEui64FromNodeId(nodeId, sourceEui) != EMBER_SUCCESS) {
    emberSerialPrintf(serialPort,"No Address Table Entry for 0x%2x\r\n", nodeId);
    return;
  }

  emberAfGetEui64(destEui); // use local EUI as destination EUI

  status = emberBindRequest(nodeId,          // who gets the bind req
                            sourceEui,       // source eui IN the binding
                            1,               // source endpoint
                            ZCL_POWER_CONFIG_CLUSTER_ID,    // cluster ID   
                            UNICAST_BINDING, // binding type
                            destEui,         // destination eui IN the binding
                            0,               // groupId for new binding
                            1,               // destination endpoint
                            EMBER_AF_DEFAULT_APS_OPTIONS);

  if (status != EMBER_SUCCESS)
    emberSerialPrintf(APP_SERIAL, "Failure to send configure binding command.\r\n");
}

// function to send a configure report command to device
// will configure the battery voltage value from the sensor.
void configureReportCommand(void) {
  uint16_t nodeId = (uint16_t)emberUnsignedCommandArgument(0);
  EmberStatus status;

  zclBufferSetup(ZCL_PROFILE_WIDE_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER,
                 (EmberAfClusterId) ZCL_POWER_CONFIG_CLUSTER_ID, // cluster id
                 ZCL_CONFIGURE_REPORTING_COMMAND_ID);
  zclBufferAddByte(EMBER_ZCL_REPORTING_DIRECTION_REPORTED);
  zclBufferAddWord(ZCL_BATTERY_VOLTAGE_ATTRIBUTE_ID);  // attribute id
  zclBufferAddByte(ZCL_INT8U_ATTRIBUTE_TYPE);           // type

  // For test purposes, I set reporting to 20 seconds.  It should be 30 
  // minutes if we are using this for a heartbeat.  30 * 60 = 1800
  zclBufferAddWord(20);  // minimum reporting interval in seconds
  zclBufferAddWord(20);  // maximum reporting interval in seconds
  zclBufferAddByte(0);   // report any change.

  // send code here.
  emAfApsFrameEndpointSetup(1, 1);
  status = emberAfSendUnicast(EMBER_OUTGOING_DIRECT,
                                nodeId,  // destination
                                &globalApsFrame,
                                appZclBufferLen,
                                appZclBuffer);

  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL, "Failure to send configure report command.\r\n");
  }

  zclCmdIsBuilt = FALSE;
}

void customCliServiceDiscoveryCallback(const EmberAfServiceDiscoveryResult* result)
{
  if (result->zdoRequestClusterId == IEEE_ADDRESS_REQUEST) {

    if (result->status == EMBER_AF_UNICAST_SERVICE_DISCOVERY_TIMEOUT) {
      emberSerialPrintf(APP_SERIAL, "Unicast Timeout\r\n");
      return;
    }
    uint8_t* eui64ptr = (uint8_t*)(result->responseData);

    newDeviceJoinHandler(discoverNode, eui64ptr);
  }
}

void discoverPresentNodeCommand(void)
{
  discoverNode = (EmberNodeId)emberUnsignedCommandArgument(0);

  emberAfFindIeeeAddress(discoverNode,
                         customCliServiceDiscoveryCallback);
}

void addressTableSend(uint16_t index)
{
  uint16_t nodeId;
  uint8_t endpoint;
  EmberStatus status;

  nodeId = addressTable[index].nodeId;
  endpoint = addressTable[index].endpoints[0];

  emAfApsFrameEndpointSetup(1, endpoint);

  status = emberAfSendUnicast(EMBER_OUTGOING_DIRECT,
                              nodeId,
                              &globalApsFrame,
                              appZclBufferLen,
                              appZclBuffer);

  emberSerialPrintf(APP_SERIAL,"Send Status %x\r\n", status);
  zclCmdIsBuilt = FALSE;
}

void addressTableSendCommand(void)
{
  uint16_t index = (uint8_t)emberUnsignedCommandArgument(0);

  addressTableSend( index );
}

EmberStatus emberAfTrustCenterStartNetworkKeyUpdate(void);

void changeNwkKeyCommand( void )
{
#ifdef EZSP_HOST
  EmberStatus status = emberAfTrustCenterStartNetworkKeyUpdate();

  if (status != EMBER_SUCCESS) {
    emberSerialPrintf(APP_SERIAL,"Change Key Error %x\r\n", status);
  } else {
    emberSerialPrintf(APP_SERIAL,"Change Key Success\r\n");
  }
#endif
}

void dcPrintKey( uint8_t label, uint8_t *key )
{
  uint8_t i;

  emberSerialPrintf(APP_SERIAL,"key %x: ", label);

  for(i=0; i< EMBER_ENCRYPTION_KEY_SIZE; i++) {
    emberSerialPrintf(serialPort, "%x", key[i]);
  }
  emberSerialPrintf(APP_SERIAL,"\r\n");
}


void printNextKeyCommand( void )
{
  EmberKeyStruct nextNwkKey;
  EmberStatus status;

  status = emberGetKey(EMBER_NEXT_NETWORK_KEY,
                       &nextNwkKey);

  if (status != EMBER_SUCCESS)
    emberSerialPrintf(APP_SERIAL, "Error getting key\r\n");
  else 
    dcPrintKey(1, nextNwkKey.key.contents);
}

void printChildTableCommand( void )
{
#ifdef EZSP_HOST
  printChildTable();
#endif
}

void killRuleCommand( void )
{
  emberAfPluginRulesEngineKillRule();
}

void channelChangeCommand( void )
{
  uint8_t channel = (uint8_t)emberUnsignedCommandArgument(0);

  if (channel < 11 || channel > 26) {
    emberSerialPrintf(APP_SERIAL, "Channel %d out of range\r\n", channel);
    return;
  }

  emberChannelChangeRequest( channel );
}

void discoverAttributesCommand( void )
{
  uint8_t server = (uint8_t)emberUnsignedCommandArgument(3);

  if (server) {
  	zclBufferSetup(ZCL_PROFILE_WIDE_COMMAND | ZCL_FRAME_CONTROL_CLIENT_TO_SERVER,
	                 (EmberAfClusterId)emberUnsignedCommandArgument(0), // cluster id
	                 ZCL_DISCOVER_ATTRIBUTES_EXTENDED_COMMAND_ID);
    zclBufferAddWordFromArgument(1); // start command id
	  zclBufferAddByteFromArgument(2); // max command ids

  } else {
  	zclBufferSetup(ZCL_PROFILE_WIDE_COMMAND | ZCL_FRAME_CONTROL_SERVER_TO_CLIENT,
	                 (EmberAfClusterId)emberUnsignedCommandArgument(0), // cluster id
	                 ZCL_DISCOVER_ATTRIBUTES_EXTENDED_COMMAND_ID);
    zclBufferAddWordFromArgument(1); // start command id
	  zclBufferAddByteFromArgument(2); // max command ids
  }

  cliBufferPrint();
}

// ******************************************
// On Off Test

static uint16_t deviceEntry, msBetweenToggles;

void onOffTestCommand( void )
{
  deviceEntry = (uint16_t)emberUnsignedCommandArgument(0);
  msBetweenToggles = (uint16_t)emberUnsignedCommandArgument(1);

  // signal "stop test" with 0 mS.

  if (msBetweenToggles == 0) {
    emberEventControlSetInactive( onOffTestEventControl );
  } else {
    emberEventControlSetActive( onOffTestEventControl );
  }
}

void emberAfPluginGatewayCommandsOnOffTestEventHandler(void)
{
  // set up next event
  emberEventControlSetDelayMS( onOffTestEventControl, msBetweenToggles );

  zclSimpleClientCommand(ZCL_ON_OFF_CLUSTER_ID, ZCL_TOGGLE_COMMAND_ID);

  deviceTableSend( deviceEntry );
}

void versionCommand( void )
{
  emberSerialPrintf(APP_SERIAL, "Version:  0.1 Alpha");
  emberSerialPrintf(APP_SERIAL, " %s", __DATE__ );
  emberSerialPrintf(APP_SERIAL, " %s", __TIME__ );
  emberSerialPrintf(APP_SERIAL, "\r\n");
}

void setTxPowerCommand( void )
{
  int8_t dBm = (int8_t)emberSignedCommandArgument(0);

  emberSetRadioPower( dBm );

}

static void customPjoinCommand( void )
{
  uint8_t time = emberSignedCommandArgument(0);
  boolean allow = (time > 0);

#if defined EZSP_HOST
  ezspSetPolicy(EZSP_TRUST_CENTER_POLICY,
                (allow
                 ? EZSP_ALLOW_PRECONFIGURED_KEY_JOINS
                 : EZSP_ALLOW_REJOINS_ONLY));
#endif
}
