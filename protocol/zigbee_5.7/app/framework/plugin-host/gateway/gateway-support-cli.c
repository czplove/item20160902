// File: gateway-support-cli.c
//
// Description:  Gateway specific CLI behavior for a host application.
//   In this case we assume our application is running on
//   a PC with Unix library support, connected to an NCP via serial uart.
//
// Copyright 2014 Silicon Laboratories, Inc.                                *80*
//
//------------------------------------------------------------------------------

#include PLATFORM_HEADER
// common include file
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
#include "app/framework/plugin/transport-mqtt/transport-mqtt.h"
#include "build/external/inc/cJSON.h"
#include "stdlib.h"
#include <time.h>

//------------------------------------------------------------------------------
// Forward Declarations

void emberAfPluginGatewaySupportBoardName(void);
void emberAfPluginGatewaySupportMfgCode(void);
void emberAfPluginGatewaySupportMfgString(void);
void emberAfPluginGatewaySupportPing(void);
void emberAfPluginGatewaySupportPjoinCommand(void);
void emberAfPluginGatewaySupportTimeSyncLocal(void);
void emberAfPluginGatewaySupportTxPower(void);
void emberAfPluginGatewaySupportVersion(void);
static char* createTwoByteHexString(uint16_t value);
char* allocateAndFormMqttGatewayTopic(char* channel);
//------------------------------------------------------------------------------
// Globals
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



//mfg custom version of the harware
#define EZSP_MFG_CUSTOM_VERSION 0x00
//mfg string used to store the project code
#define EZSP_MFG_STRING 0x01
//mfg board name used to store the name of the hardware
#define EZSP_MFG_BOARD_NAME 0x02
//mfg manufactuer Id  id the manufactuer code distributed  by ZigBee alliance  
#define EZSP_MFG_MANUF_ID 0x03

#define EZSP_MFG_PHY_CONFIG 0x04

#define EZSP_MFG_BOOTLOAD_AES_KEY 0x05

#define EZSP_MFG_ASH_CONFIG 0x06

#define EZSP_MFG_EZSP_STORAGE 0x07

#define EZSP_STACK_CAL_DATA 0x08

#define EZSP_MFG_CBKE_DATA 0x09

#define EZSP_MFG_INSTALLATION_CODE 0x0A
// The difference in seconds between the ZigBee Epoch: January 1st, 2000
// and the Unix Epoch: January 1st 1970.
#define UNIX_ZIGBEE_EPOCH_DELTA (uint32_t)946684800UL

#if !defined(EMBER_AF_GENERATE_CLI)

EmberCommandEntry emberAfPluginGatewayCommands[] = {
  emberCommandEntryAction("time-sync-local", 
                          emberAfPluginGatewaySupportTimeSyncLocal, 
                          "",
                          "This command retrieves the local unix time and syncs the Time Server attribute to it."),
  emberCommandEntryTerminator(),
};

#endif

//------------------------------------------------------------------------------
// Functions

void emberAfPluginGatewaySupportTimeSyncLocal(void)
{
  time_t unixTime = time(NULL);
  unixTime -= UNIX_ZIGBEE_EPOCH_DELTA;
  emberAfSetTime(unixTime);
  emberAfPrintTime(emberAfGetCurrentTime());
}
void emberAfPluginGatewaySupportBoardName(void)
{
#if defined EZSP_HOST
  uint8_t i,tokenData[16];
  uint8_t tokenLength =ezspGetMfgToken(EZSP_MFG_BOARD_NAME, tokenData);
  for(i = 0; i <tokenLength; i++)
	  if(tokenData[i] ==0xFF)
		  tokenData[i] = 0;
  emberAfAppPrint("EZSP_MFG_BOARD_NAME:[0x%x]  %p\r\n", tokenLength,tokenData);
#endif
}
void emberAfPluginGatewaySupportMfgCode(void)
{
#if defined EZSP_HOST
  uint8_t tokenData[2];
  uint16_t *pMfgCode = (uint16_t *)tokenData;
  uint8_t tokenLength = ezspGetMfgToken(EZSP_MFG_MANUF_ID, tokenData);
  emberAfAppPrint("EZSP_MFG_MANUF_ID: 0x%2X\r\n",*pMfgCode);
#endif
}
void emberAfPluginGatewaySupportTxPower(void)
{
	int8_t dBm = (int8_t)emberSignedCommandArgument(0);
	emberSetRadioPower( dBm );
}
void emberAfPluginGatewaySupportVersion(void)
{
  emberAfAppPrint("Version:  0.1 Alpha");
  emberAfAppPrint(" %s", __DATE__ );
  emberAfAppPrintln(" %s", __TIME__ );
}

void emberAfPluginGatewaySupportMfgString(void)
{
#if defined EZSP_HOST
  uint8_t i,tokenData[16];
  uint8_t tokenLength =ezspGetMfgToken(EZSP_MFG_STRING, tokenData);
  for(i = 0; i <tokenLength; i++)
	  if(tokenData[i] ==0xFF)
		  tokenData[i] = 0;
  emberAfAppPrint("EZSP_MFG_STRING:[0x%x]  %p\r\n", tokenLength,tokenData);
#endif
}

void emberAfPluginGatewaySupportPjoinCommand(void)
{
	uint8_t time = emberUnsignedCommandArgument(0);
	boolean allow = (time > 0);

#if defined EZSP_HOST
	ezspSetPolicy(EZSP_TRUST_CENTER_POLICY,
                (allow
                 ? EZSP_ALLOW_PRECONFIGURED_KEY_JOINS
                 : EZSP_ALLOW_REJOINS_ONLY));
#endif
}

void emberAfPluginGatewaySupportPing(void)
{
  char* topic = allocateAndFormMqttGatewayTopic("pingresponse");
  cJSON* pingResponseJson;
  char* pingResponseString;
  char* tempString;
  uint8_t i,tokenData[16];
  uint8_t tokenLength =ezspGetMfgToken(EZSP_MFG_BOARD_NAME, tokenData);
  for(i = 0; i <tokenLength; i++)
	  if(tokenData[i] ==0xFF)
		  tokenData[i] = 0;
  pingResponseJson = cJSON_CreateObject();
  cJSON_AddStringToObject(pingResponseJson, "boardName", (char*)tokenData);
  tokenLength =ezspGetMfgToken(EZSP_MFG_STRING, tokenData);
  for(i = 0; i <tokenLength; i++)
	  if(tokenData[i] ==0xFF)
		  tokenData[i] = 0;  
  cJSON_AddStringToObject(pingResponseJson, "string", (char*)tokenData);
  uint16_t *pMfgCode = (uint16_t *)tokenData;
  tokenLength = ezspGetMfgToken(EZSP_MFG_MANUF_ID, tokenData);
  tempString = createTwoByteHexString(*pMfgCode);
  cJSON_AddStringToObject(pingResponseJson,"mfgCode",tempString);
  free(tempString);
  uint16_t *gwVersion = (uint16_t *)tokenData;
  tokenLength = ezspGetMfgToken(EZSP_MFG_CUSTOM_VERSION, tokenData);
  cJSON_AddIntegerToObject(pingResponseJson,"gwVersion",*gwVersion);
  pingResponseString = cJSON_PrintUnformatted(pingResponseJson);
  emberAfPluginTransportMqttPublish(topic, pingResponseString);  
  free(pingResponseString);
  cJSON_Delete(pingResponseJson);
  free(topic); 
}
static char* createTwoByteHexString(uint16_t value)
{
  char* outputString = (char *) malloc(7);
  
  sprintf(outputString, "%04X", value);
  
  return outputString;
}