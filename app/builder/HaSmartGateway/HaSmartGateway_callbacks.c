// Copyright 2016 Silicon Laboratories, Inc.                                *80*

// This callback file is created for your convenience. You may add application code
// to this file. If you regenerate this file over a previous version, the previous
// version will be overwritten and any code you have added will be lost.

#include "app/framework/include/af.h"
#include "app/framework/util/af-main.h"
#include "app/framework/util/util.h"
#include "app/framework/plugin/concentrator/concentrator-support.h"
#include "app/framework/plugin/device-table/device-table.h"
#include "app/framework/plugin/rules-engine/rules-engine.h"
#include "app/framework/plugin/transport-mqtt/transport-mqtt.h"
#include "app/framework/plugin/ota-server/ota-server.h"
#include "app/framework/plugin/traffic-test/traffic-test.h"
#include "app/framework/plugin/network-creator/network-creator.h"
#include "sys/time.h"
#include "stdlib.h"
#include "app/framework/plugin-host/link-list/link-list.h"
#ifndef EMBEDDED_GATEWAY
  #include <stdio.h>
#endif
// cJSON open-source-module include
#include "build/external/inc/cJSON.h"

// stuff from 5.3 that is not in 5.1
#define ZCL_BYPASS_RESPONSE_COMMAND_ID 0x07    // Ver.: since ha-1.2.1-05-3520-30
#define ZCL_GET_ZONE_STATUS_RESPONSE_COMMAND_ID 0x08    // Ver.: since ha-1.2.1-05-3520-30

// Constants
#define HOSTVER_STRING_LENGTH 14 // 13 characters + NULL (99.99.99-9999)
#define EUI64_STRING_LENGTH 17 // 16 characters + NULL
#define NODEID_STRING_LENGTH 5 // 4 characters + NULL
#define PANID_STRING_LENGTH 5 // 4 chars + NULL
#define CLUSTERID_STRING_LENGTH 5 // 4 chars + NULL
#define GATEWAY_TOPIC_PREFIX_LENGTH 21 // 21 chars `gw/xxxxxxxxxxxxxxxx/` + NULL
#define HEARTBEAT_RATE_MS 600000 // milliseconds
#define PROCESS_COMMAND_RATE_MS 20 // milliseconds
#define BLOCK_SENT_THROTTLE_VALUE 25 // blocks to receive before sending updates

// Attribute reading buffer location definitions
#define ATTRIBUTE_BUFFER_CLUSTERID_HIGH_BITS 1
#define ATTRIBUTE_BUFFER_CLUSTERID_LOW_BITS  0
#define ATTRIBUTE_BUFFER_SUCCESS_CODE        2
#define ATTRIBUTE_BUFFER_DATA_TYPE           3
#define ATTRIBUTE_BUFFER_DATA_START          4

// Attribute reporting / IAS ZONE buffer location definitions
#define ATTRIBUTE_BUFFER_REPORT_DATA_START          3
#define ATTRIBUTE_BUFFER_REPORT_CLUSTERID_HIGH_BITS 1
#define ATTRIBUTE_BUFFER_REPORT_CLUSTERID_LOW_BITS  0
#define ATTRIBUTE_BUFFER_REPORT_DATA_TYPE           2

#define COMMAND_OFFSET 2
#define ZCL_RESPONSE_TOPIC "zclresponse"
#define ZDO_RESPONSE_TOPIC "zdoresponse"
#define APS_RESPONSE_TOPIC "apsresponse"

#define ONE_BYTE_HEX_STRING_SIZE 5
#define TWO_BYTE_HEX_STRING_SIZE 7

#define IAS_ZONE_CONTACT_STATE_BIT 0
#define IAS_ZONE_TAMPER_STATE_BIT  2
#define IAS_ZONE_BATTERY_STATE_BIT 3

#define READ_REPORT_CONFIG_STATUS       0
#define READ_REPORT_CONFIG_DIRECTION    1
#define READ_REPORT_CONFIG_ATTRIBUTE_ID 2
#define READ_REPORT_CONFIG_DATA_TYPE    4
#define READ_REPORT_CONFIG_MIN_INTERVAL 5
#define READ_REPORT_CONFIG_MAX_INTERVAL 7
#define READ_REPORT_CONFIG_DATA         9

#define DEVICE_TABLE_BIND_RESPONSE_STATUS 1

#define BINDING_TABLE_RESPONSE_NUM_ENTRIES 4
#define BINDING_TABLE_RESPONSE_STATUS      1
#define BINDING_TABLE_RESPONSE_ENTRIES     5
#define BINDING_TABLE_RESPONSE_ENTRY_SIZE  21
#define BINDING_ENTRY_EUI                0
#define BINDING_ENTRY_SOURCE_ENDPOINT    8
#define BINDING_ENTRY_CLUSTER_ID         9
#define BINDING_ENTRY_ADDRESS_MODE       11
#define BINDING_ENTRY_DEST_EUI           12
#define BINDING_ENTRY_DEST_ENDPOINT      20

// Gateway global variables
static EmberEUI64 gatewayEui64;
static char gatewayEui64String[EUI64_STRING_LENGTH] = {0};
static char gatewayTopicUriPrefix[GATEWAY_TOPIC_PREFIX_LENGTH] = {0};
static bool trafficReporting = false;
static int32u otaBlockSent = 0; // This only supports one device OTA at a time

// Command list global variables
// We need to keep a list of commands to process as the come in from an external
// source to the gateway, these are the structs, defines and list needed for that
#define COMMAND_TYPE_CLI        0x01
#define COMMAND_TYPE_POST_DELAY 0x02

typedef struct _GatewayCommand {
  uint8_t commandType;
  char* cliCommand; // used for COMMAND_TYPE_CLI
  int32u postDelayMs; // used for COMMAND_TYPE_POST_DELAY
  int64u resumeTime; // used for COMMAND_TYPE_POST_DELAY
} GatewayCommand;

static LinkList* commandList;

static GatewayCommand* allocateGatewayCommand();
static void freeGatewayCommand(GatewayCommand* gatewayCommand);
static void addCliCommandToList(LinkList* commandList, const char* cliCommand);
static void addPostDelayMsToList(LinkList* commandList, int32u postDelayMs);

// This is used in the traffic test results
extern uint8_t appZclBuffer[];
#define APP_ZCL_BUFFER_SEQUENCE_INDEX 1 // found from zclBufferSetup()

// Route repair global variables
enum {
  REPAIR_STATE_IDLE             = 0x00,
  REPAIR_STATE_MTORR_SENT       = 0x01,
  REPAIR_STATE_NWK_REQUEST_SENT = 0x02
};
uint8_t routeRepairState = REPAIR_STATE_IDLE;
EmberNodeId currentRepairNodeId;

// Events
EmberEventControl heartbeatEventControl;
void heartbeatEventFunction(void);

EmberEventControl processCommandEventControl;
void processCommandEventFunction(void);

EmberEventControl stateUpdateEventControl;
void stateUpdateEventFunction(void);

EmberEventControl routeRepairEventControl;
void routeRepairEventFunction(void);

// String/other helpers
static void nodeIdToString(EmberNodeId nodeId, char* nodeIdString);
static void eui64ToString(EmberEUI64 eui, char* euiString);
static int64u getTimeMS();
static uint8_t getAdjustedSequenceNumber();
static void printAttributeBuffer(uint16_t clusterId, uint8_t* buffer, uint16_t bufLen);

// MQTT API publishing helpers
static char* allocateAndFormMqttGatewayTopic(char* channel);
static void publishMqttHeartbeat(void);
static void publishMqttRules(void);
static void publishMqttGatewayState(void);
static void publishMqttContactSense(EmberNodeId nodeId,
                                    EmberEUI64 eui64,
                                    uint8_t contactState,
                                    uint8_t tamperState);
static void publishMqttSwitchEvent(EmberNodeId nodeId,
                                    EmberEUI64 eui64,
                                    char* eventString);
static void publishMqttDeviceStateChange(EmberNodeId nodeId,
                                         EmberEUI64 eui64,
                                         uint8_t state);
static void publishMqttDeviceJoined(EmberEUI64 eui64);
static void publishMqttDeviceLeft(EmberEUI64 eui64);
static void publishMqttTrafficTestResult(uint16_t numDefaultResponses,
                                         uint16_t numTxSendErrors,
                                         uint16_t numTotalMessages);
static void publishMqttNodeHeartBeat(EmberNodeId nodeId,
                                 EmberEUI64 eui64,
                                 EmberAfClusterId clusterId,
                                 uint8_t* buffer,
                                 uint16_t bufLen);
static void publishMqttAttribute(EmberNodeId nodeId,
                                 EmberEUI64 eui64,
                                 EmberAfClusterId clusterId,
                                 uint8_t* buffer,
                                 uint16_t bufLen);
static void publishMqttTrafficReportEvent(char* messageType,
                                          EmberStatus* status,
                                          int8_t* lastHopRssi,
                                          uint8_t* lastHopLqi,
                                          uint8_t* zclSequenceNumber,
                                          int64u timeMS);
static void publishMqttOtaEvent(char* messageType,
                                EmberNodeId nodeId,
                                EmberEUI64 eui64,
                                uint8_t* status,
                                int32u* blockSent,
                                uint8_t* actualLength,
                                uint16_t* manufacturerId,
                                uint16_t* imageTypeId,
                                int32u* firmwareVersion);
static void publishMqttCommandExecuted(char* cliCommand);
static void publishMqttDelayExecuted(int32u postDelayMs);
static cJSON* buildNodeJson(EmberNodeId nodeId);

// MQTT topic and handler list
typedef void (*MqttTopicHandler)(cJSON* messageJson);

typedef struct _MqttTopicHandlerMap
{
  char* topic;
  MqttTopicHandler topicHandler;
} MqttTopicHandlerMap;

LinkList* topicHandlerList;

MqttTopicHandlerMap* buildTopicHandler(char* topicString,
                                       MqttTopicHandler topicHandlerFunction);

// MQTT API subscription helpers
static void handleCommandsMessage(cJSON* messageJson);
static void handlePublishStateMessage(cJSON* messageJson);
static void handleUpdateSettingsMessage(cJSON* messageJson);

// Handy string creation routines.
static char* createOneByteHexString(uint8_t value)
{
  char* outputString = (char *) malloc(ONE_BYTE_HEX_STRING_SIZE);
  
  sprintf(outputString, "0x%02X", value);
  
  return outputString;
}

static char* createTwoByteHexString(uint16_t value)
{
  char* outputString = (char *) malloc(TWO_BYTE_HEX_STRING_SIZE);
  
  sprintf(outputString, "0x%04X", value);
  
  return outputString;
}

/** @brief Main Init
 *
 * This function is called from the application’s main function. It gives the
 * application a chance to do any initialization required at system startup.
 * Any code that you would normally put into the top of the application’s
 * main() routine should be put into this function.
        Note: No callback
 * in the Application Framework is associated with resource cleanup. If you
 * are implementing your application on a Unix host where resource cleanup is
 * a consideration, we expect that you will use the standard Posix system
 * calls, including the use of atexit() and handlers for signals such as
 * SIGTERM, SIGINT, SIGCHLD, SIGPIPE and so on. If you use the signal()
 * function to register your signal handler, please mind the returned value
 * which may be an Application Framework function. If the return value is
 * non-null, please make sure that you call the returned function from your
 * handler to avoid negating the resource cleanup of the Application Framework
 * itself.
 *
 */
void emberAfMainInitCallback(void)
{
  EmberNodeType nodeTypeResult = 0xFF;
  EmberNetworkParameters networkParams;
  cJSON* heartbeatJson;
  char* heartbeatJsonString;
  char panIdString[7] = {0};
  char nodeIdString[7] = {0};
  char xpanString[17] = {0};
  emberSerialPrintfLine(APP_SERIAL, "HA Gateweay Init");

  // Save our EUI and network information
  emberAfGetEui64(gatewayEui64);
  emberAfGetNetworkParameters(&nodeTypeResult, &networkParams);
  eui64ToString(gatewayEui64, gatewayEui64String);
  eui64ToString(networkParams.extendedPanId, xpanString);
  nodeIdToString(emberAfGetNodeId(),nodeIdString);
  nodeIdToString(networkParams.panId,panIdString);


  heartbeatJson = cJSON_CreateObject();
  cJSON_AddStringToObject(heartbeatJson, "gatewayEui",gatewayEui64String);
  cJSON_AddStringToObject(heartbeatJson, "nodeID",nodeIdString);
  cJSON_AddStringToObject(heartbeatJson, "panID",panIdString);
  cJSON_AddIntegerToObject(heartbeatJson, "channel",networkParams.radioChannel);
  cJSON_AddIntegerToObject(heartbeatJson, "radioTxPower",networkParams.radioTxPower);
  cJSON_AddStringToObject(heartbeatJson, "xpan",xpanString);
  cJSON_AddIntegerToObject(heartbeatJson, "nodeType",nodeTypeResult);
  cJSON_AddIntegerToObject(heartbeatJson, "Security level",emberAfGetSecurityLevel());
  cJSON_AddIntegerToObject(heartbeatJson, "network state",emberNetworkState());
  heartbeatJsonString = cJSON_PrintUnformatted(heartbeatJson);
#ifndef EMBEDDED_GATEWAY
  FILE *fp;
  uint16_t i;

  // save EUI address into file
  fp = fopen("/etc/env/gwcfg.json", "w");
        fprintf(fp, "%s ",heartbeatJsonString);
  // write something to mark the end
  //fprintf(fp, "\r\nffff\r\n");
  fclose(fp);
 #endif
  free(heartbeatJsonString);
  cJSON_Delete(heartbeatJson);

  strcat(gatewayTopicUriPrefix, "gw/");
  strcat(gatewayTopicUriPrefix, gatewayEui64String);
  strcat(gatewayTopicUriPrefix, "/");
  emberSerialPrintfLine(APP_SERIAL,
                        "HA Gateweay EUI64 = %s",
                        gatewayEui64String);

  // Init our command list
  commandList = linkListInit();

  // Init our topic handler list and the maps in the list, note that this is
  // done after the topicUriPrefix is assigned above, since it is used here
  topicHandlerList = linkListInit();
  linkListPushBack(topicHandlerList,
                   (void*)buildTopicHandler("commands",
                                            handleCommandsMessage));
  linkListPushBack(topicHandlerList,
                   (void*)buildTopicHandler("publishstate",
                                            handlePublishStateMessage));
  linkListPushBack(topicHandlerList,
                   (void*)buildTopicHandler("updatesettings",
                                            handleUpdateSettingsMessage));
}

MqttTopicHandlerMap* buildTopicHandler(char* topicString,
                                       MqttTopicHandler topicHandlerFunction)
{
  MqttTopicHandlerMap* topicHandlerMap =
    (MqttTopicHandlerMap*)malloc(sizeof(MqttTopicHandlerMap));
  topicHandlerMap->topic = allocateAndFormMqttGatewayTopic(topicString);
  topicHandlerMap->topicHandler = topicHandlerFunction;
  return topicHandlerMap;
}

/** @brief Get Radio Power For Channel
 *
 * This callback is called by the framework when it is setting the radio power
 * during the discovery process. The framework will set the radio power
 * depending on what is returned by this callback.
 *
 * @param channel   Ver.: always
 */
int8_t emberAfPluginNetworkFindGetRadioPowerForChannelCallback(uint8_t channel)
{
  return EMBER_AF_PLUGIN_NETWORK_FIND_RADIO_TX_POWER;
}

/** @brief Join
 *
 * This callback is called by the plugin when a joinable network has been
 * found.  If the application returns true, the plugin will attempt to join
 * the network.  Otherwise, the plugin will ignore the network and continue
 * searching.  Applications can use this callback to implement a network
 * blacklist.
 *
 * @param networkFound   Ver.: always
 * @param lqi   Ver.: always
 * @param rssi   Ver.: always
 */
bool emberAfPluginNetworkFindJoinCallback(EmberZigbeeNetwork * networkFound,
                                             uint8_t lqi,
                                             int8_t rssi)
{
  return true;
}

/** @brief Request Mirror
 *
 * This function is called by the Simple Metering client plugin whenever a
 * Request Mirror command is received.  The application should return the
 * endpoint to which the mirror has been assigned.  If no mirror could be
 * assigned, the application should return 0xFFFF.
 *
 * @param requestingDeviceIeeeAddress   Ver.: always
 */
uint16_t emberAfPluginSimpleMeteringClientRequestMirrorCallback(EmberEUI64 requestingDeviceIeeeAddress)
{
  return 0xFFFF;
}

/** @brief Remove Mirror
 *
 * This function is called by the Simple Metering client plugin whenever a
 * Remove Mirror command is received.  The application should return the
 * endpoint on which the mirror has been removed.  If the mirror could not be
 * removed, the application should return 0xFFFF.
 *
 * @param requestingDeviceIeeeAddress   Ver.: always
 */
uint16_t emberAfPluginSimpleMeteringClientRemoveMirrorCallback(EmberEUI64 requestingDeviceIeeeAddress)
{
  return 0xFFFF;
}

/** @brief Select File Descriptors
 *
 * This function is called when the Gateway plugin will do a select() call to
 * yield the processor until it has a timed event that needs to execute.  The
 * function implementor may add additional file descriptors that the
 * application will monitor with select() for data ready.  These file
 * descriptors must be read file descriptors.  The number of file descriptors
 * added must be returned by the function (0 for none added).
 *
 * @param list A pointer to a list of File descriptors that the function
 * implementor may append to  Ver.: always
 * @param maxSize The maximum number of elements that the function implementor
 * may add.  Ver.: always
 */
int emberAfPluginGatewaySelectFileDescriptorsCallback(int* list,
                                                      int maxSize)
{
  return 0;
}

// Route Repair functions
void initiateRouteRepair(EmberNodeId nodeId)
{
  if (routeRepairState == REPAIR_STATE_IDLE) {
    emberEventControlSetActive(routeRepairEventControl);
    currentRepairNodeId = nodeId;
  }
}

static void serviceReturn(const EmberAfServiceDiscoveryResult* result)
{
  emberSerialPrintfLine(APP_SERIAL,
                    "ROUTE REPAIR SERVICE RETURN RESULT: status=%d",
                    result->status);

  if (result->status == EMBER_AF_UNICAST_SERVICE_DISCOVERY_TIMEOUT) {
    routeRepairState = REPAIR_STATE_IDLE;
  }

  if (routeRepairState != REPAIR_STATE_IDLE) {
    emberEventControlSetActive(routeRepairEventControl);
  }
}

void routeRepairEventFunction(void) {
  uint16_t timeQS;
  EmberEUI64 eui64;

  emberSerialPrintfLine(APP_SERIAL,
                        "ROUTE REPAIR: state=%x nodeId=%2x", 
                        routeRepairState, 
                        currentRepairNodeId);

  switch (routeRepairState) {
  case REPAIR_STATE_IDLE:
    timeQS = (uint16_t)emberAfPluginConcentratorQueueDiscovery();
    if (timeQS > 20) {
      timeQS = 20;
    }
    emberEventControlSetDelayQS(routeRepairEventControl, (timeQS + 2));
    routeRepairState = REPAIR_STATE_MTORR_SENT;
    break;
  case REPAIR_STATE_MTORR_SENT:
    // MTORR should have been sent by now. Time to send the query.  
    // Send out network address request.
    getIeeeFromNodeId(currentRepairNodeId, eui64);
    emberAfFindNodeId(eui64, serviceReturn);
    emberEventControlSetDelayQS(routeRepairEventControl, 8);
    routeRepairState = REPAIR_STATE_NWK_REQUEST_SENT;
    break;
  case REPAIR_STATE_NWK_REQUEST_SENT:
    emberEventControlSetInactive(routeRepairEventControl);
    routeRepairState = REPAIR_STATE_IDLE;
    break;
  default:
    // Should never get here.
    assert(0);
    break;
  }
}


// IAS ACE Server side callbacks

/** @brief Arm
 *
 *
 * @param armMode   Ver.: always
 * @param armDisarmCode   Ver.: since ha-1.2-05-3520-29
 * @param zoneId   Ver.: since ha-1.2-05-3520-29
 */
bool emberAfIasAceClusterArmCallback(uint8_t armMode,
                                        uint8_t* armDisarmCode,
                                        uint8_t zoneId)
{
  uint16_t i;
  uint16_t armDisarmCodeLength = emberAfStringLength(armDisarmCode);
  EmberNodeId sender = emberGetSender();

  emberSerialPrintf(APP_SERIAL, "IAS ACE Arm Received %x", armMode);

  // Start i at 1 to skip over leading character in the byte array as it is the
  // length byte
  for (i = 1; i < armDisarmCodeLength; i++) {
    emberSerialPrintf(APP_SERIAL, "%c", armDisarmCode[i]);
  }
  emberSerialPrintfLine(APP_SERIAL, " %x", zoneId);

  emberAfFillCommandIasAceClusterArmResponse(armMode);
  emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT, sender);

  return true;
}

/** @brief Bypass
 *
 *
 * @param numberOfZones   Ver.: always
 * @param zoneIds   Ver.: always
 */
bool emberAfIasAceClusterBypassCallback(uint8_t numberOfZones,
                                           uint8_t* zoneIds,
                                           uint8_t* armDisarmCode)
{
  EmberNodeId sender = emberGetSender();
  uint8_t i;

  emberSerialPrintf(APP_SERIAL, "IAS ACE Cluster Bypass for zones ");

  for (i=0; i<numberOfZones; i++) {
    emberSerialPrintf(APP_SERIAL, "%d ", zoneIds[i]);
  }
  emberSerialPrintfLine(APP_SERIAL, "");

  emberAfFillCommandIasAceClusterBypassResponse(numberOfZones,
                                                zoneIds,
                                                numberOfZones);
  emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT, sender);

  return true;
}

/** @brief Emergency
 *
 *
 */
bool emberAfIasAceClusterEmergencyCallback(void)
{
  return false;
}

/** @brief Fire
 *
 *
 */
bool emberAfIasAceClusterFireCallback(void)
{
  return false;
}

/** @brief Get Zone Id Map
 *
 *
 */
bool emberAfIasAceClusterGetZoneIdMapCallback(void)
{
  return false;
}

/** @brief Get Zone Information
 *
 *
 * @param zoneId   Ver.: always
 */
bool emberAfIasAceClusterGetZoneInformationCallback(uint8_t zoneId)
{
  return false;
}

/** @brief Panic
 *
 *
 */
bool emberAfIasAceClusterPanicCallback(void)
{
  return false;
}

// *************************************
// IAS ACE Client side callbacks

/** @brief Arm Response
 *
 *
 * @param armNotification   Ver.: always
 */
bool emberAfIasAceClusterArmResponseCallback(uint8_t armNotification)
{
  return false;
}

/** @brief Get Zone Id Map Response
 *
 *
 * @param section0   Ver.: always
 * @param section1   Ver.: always
 * @param section2   Ver.: always
 * @param section3   Ver.: always
 * @param section4   Ver.: always
 * @param section5   Ver.: always
 * @param section6   Ver.: always
 * @param section7   Ver.: always
 * @param section8   Ver.: always
 * @param section9   Ver.: always
 * @param section10   Ver.: always
 * @param section11   Ver.: always
 * @param section12   Ver.: always
 * @param section13   Ver.: always
 * @param section14   Ver.: always
 * @param section15   Ver.: always
 */
bool emberAfIasAceClusterGetZoneIdMapResponseCallback(uint16_t section0,
                                                         uint16_t section1,
                                                         uint16_t section2,
                                                         uint16_t section3,
                                                         uint16_t section4,
                                                         uint16_t section5,
                                                         uint16_t section6,
                                                         uint16_t section7,
                                                         uint16_t section8,
                                                         uint16_t section9,
                                                         uint16_t section10,
                                                         uint16_t section11,
                                                         uint16_t section12,
                                                         uint16_t section13,
                                                         uint16_t section14,
                                                         uint16_t section15)
{
  return false;
}

/** @brief Get Zone Information Response
 *
 *
 * @param zoneId   Ver.: always
 * @param zoneType   Ver.: always
 * @param ieeeAddress   Ver.: always
 */
bool emberAfIasAceClusterGetZoneInformationResponseCallback(uint8_t zoneId,
                                                               uint16_t zoneType,
                                                               uint8_t* ieeeAddress,
                                                               uint8_t* zoneLabel)
{
  return false;
}

/** @brief Panel Status Changed
 *
 *
 * @param panelStatus   Ver.: always
 * @param secondsRemaining   Ver.: always
 */
bool emberAfIasAceClusterPanelStatusChangedCallback(uint8_t panelStatus,
                                                       uint8_t secondsRemaining,
                                                       uint8_t audibleNotification,
                                                       uint8_t alarmStatus)
{
  return false;
}

/** @brief Zone Status Changed
 *
 *
 * @param zoneId   Ver.: always
 * @param zoneStatus   Ver.: always
 * @param zoneLabel   Ver.: always
 */
bool emberAfIasAceClusterZoneStatusChangedCallback(uint8_t zoneId,
                                                      uint16_t zoneStatus,
                                                      uint8_t audibleNotification,
                                                      uint8_t* zoneLabel)
{
  return false;
}

void heartbeatEventFunction(void) 
{
  publishMqttHeartbeat();
  emberEventControlSetDelayMS(heartbeatEventControl, HEARTBEAT_RATE_MS);
}

// Rules Engine Callbacks
void emberAfPluginRulesEnginePreCommandCallback(EmberAfClusterCommand* cmd) {
  EmberEUI64 nodeEui64;
  EmberNodeId nodeId = emberAfCurrentCommand()->source;
  getIeeeFromNodeId(nodeId, nodeEui64);

  uint8_t stepMode, hueDirection, tempDirection; 

  switch (cmd->apsFrame->clusterId) {
    case ZCL_ON_OFF_CLUSTER_ID:
      if (cmd->commandId < 0x03) {
        if(cmd->commandId == 0x00) {
          publishMqttSwitchEvent(nodeId, nodeEui64, "off");
        } else {
          publishMqttSwitchEvent(nodeId, nodeEui64, "on");
        }
      }
      break;
    case ZCL_IAS_ZONE_CLUSTER_ID:
      break;
    case ZCL_LEVEL_CONTROL_CLUSTER_ID:
      stepMode = cmd->buffer[cmd->payloadStartIndex + 0];
      if (stepMode == 0) {
        publishMqttSwitchEvent(nodeId, nodeEui64, "levelup");
      } else {
        publishMqttSwitchEvent(nodeId, nodeEui64, "leveldown");
      }
      break;
    case ZCL_COLOR_CONTROL_CLUSTER_ID:
      switch (cmd->commandId) {
        case ZCL_STEP_HUE_COMMAND_ID:
          hueDirection = cmd->buffer[cmd->payloadStartIndex + 0];
          if (hueDirection == 1) {
            publishMqttSwitchEvent(nodeId, nodeEui64, "hueup");
          } else {
            publishMqttSwitchEvent(nodeId, nodeEui64, "huedown");
          }
          break;
        case ZCL_STEP_COLOR_COMMAND_ID:
          // ignore for now
          break;
        case ZCL_STEP_COLOR_TEMPERATUE_COMMAND_ID:
          tempDirection = cmd->buffer[cmd->payloadStartIndex + 0];
          if (tempDirection == 1) {
            publishMqttSwitchEvent(nodeId, nodeEui64, "tempup");
          } else {
            publishMqttSwitchEvent(nodeId, nodeEui64, "tempdown");
          }
          break;
        case ZCL_MOVE_TO_HUE_COMMAND_ID:
          // ignore for now
          break;
        case ZCL_MOVE_TO_HUE_AND_SATURATION_COMMAND_ID:
          // ignore for now
          break;
        case ZCL_MOVE_TO_COLOR_COMMAND_ID:
          // ignore for now
          break;
        case ZCL_MOVE_TO_COLOR_TEMPERATURE_COMMAND_ID:
          // ignore for now
          break;
        default:
          break;
      }
    }
};

// MQTT Helper Functions
static char* allocateAndFormMqttGatewayTopic(char* topic)
{
  // Add our string sizes + one NULL char
  uint16_t stringSize = strlen(gatewayTopicUriPrefix) + strlen(topic) + 1;
  char* fullTopicUri = (char*)malloc(stringSize);
  memset(fullTopicUri, 0, stringSize);
  strcat(fullTopicUri, gatewayTopicUriPrefix);
  strcat(fullTopicUri, topic);
  return fullTopicUri;
}

static void publishMqttHeartbeat(void)
{
  char* topic = allocateAndFormMqttGatewayTopic("heartbeat");
  EmberNetworkParameters parameters;
  EmberNodeType nodeType;
  cJSON* heartbeatJson;
  char* heartbeatJsonString;
  char panIdString[PANID_STRING_LENGTH] = {0};

  EmberStatus status = emberAfGetNetworkParameters(&nodeType, &parameters);
  sprintf(panIdString, "%04X", parameters.panId);

  heartbeatJson = cJSON_CreateObject();

  if (!emberAfNcpNeedsReset() && status == EMBER_SUCCESS) {
    cJSON_AddStringToObject(heartbeatJson, "networkState", "up");
    cJSON_AddStringToObject(heartbeatJson, "networkPanId", panIdString);
    cJSON_AddIntegerToObject(heartbeatJson, "radioTxPower", parameters.radioTxPower);
    cJSON_AddIntegerToObject(heartbeatJson, "radioChannel", parameters.radioChannel);
  } else {
    cJSON_AddStringToObject(heartbeatJson, "networkState", "down");
  }

  heartbeatJsonString = cJSON_PrintUnformatted(heartbeatJson);
  emberAfPluginTransportMqttPublish(topic, heartbeatJsonString);
  free(heartbeatJsonString);
  cJSON_Delete(heartbeatJson);
  free(topic);
}

static void publishMqttDevices(void)
{
  char* topic = allocateAndFormMqttGatewayTopic("devices");
  uint16_t nodeIndex;
  cJSON* nodeJson;
  cJSON* devicesJson;
  cJSON* devicesJsonNodeArray;
  char* devicesJsonString;

  devicesJson = cJSON_CreateObject();
  devicesJsonNodeArray = cJSON_CreateArray();
  cJSON_AddItemToObject(devicesJson, "devices", devicesJsonNodeArray);

  for (nodeIndex = 0; nodeIndex < ADDRESS_TABLE_SIZE; nodeIndex++) {
    if (addressTable[nodeIndex].nodeId != NULL_NODE_ID) {
      nodeJson = buildNodeJson(addressTable[nodeIndex].nodeId);
      cJSON_AddItemToArray(devicesJsonNodeArray, nodeJson);
    }
  }

  devicesJsonString = cJSON_PrintUnformatted(devicesJson);
  emberAfPluginTransportMqttPublish(topic, devicesJsonString);
  free(devicesJsonString);
  cJSON_Delete(devicesJson);
  free(topic);
}

static cJSON* buildNodeJson(EmberNodeId nodeId)
{
  uint16_t addressTableIndex, epIndex;
  uint16_t endpointNum;
  cJSON* nodeJson;
  cJSON* nodeJsonEndpointArray;
  cJSON* endpointJson;
  EmberEUI64 nodeEui64;
  char euiString[EUI64_STRING_LENGTH] = {0};
  char nodeIdString[NODEID_STRING_LENGTH] = {0};

  nodeIdToString(nodeId, nodeIdString);
  getIeeeFromNodeId(nodeId, nodeEui64);
  eui64ToString(nodeEui64, euiString);
  addressTableIndex = getAddressIndexFromIeee(nodeEui64);

  assert(addressTable[addressTableIndex].nodeId == nodeId);

  nodeJson = cJSON_CreateObject();
  cJSON_AddStringToObject(nodeJson, "nodeEui", euiString);
  cJSON_AddStringToObject(nodeJson, "nodeId", nodeIdString);
  cJSON_AddIntegerToObject(nodeJson, "nodeCapabilities", addressTable[addressTableIndex].capabilities);
  cJSON_AddIntegerToObject(nodeJson, "addressTableIndex", addressTableIndex);
  cJSON_AddIntegerToObject(nodeJson, "deviceState", addressTable[addressTableIndex].state);
  nodeJsonEndpointArray = cJSON_CreateArray();
  cJSON_AddItemToObject(nodeJson, "endpoints", nodeJsonEndpointArray);
  for (epIndex = 0; epIndex < addressTable[addressTableIndex].endpointCount; epIndex++) {
    endpointNum = getEndpointFromNodeIdAndEndpoint(addressTable[addressTableIndex].nodeId,
                                                   addressTable[addressTableIndex].endpoints[epIndex]);
    if (endpointNum != NULL_INDEX) {

      endpointJson = cJSON_CreateObject();
      cJSON_AddIntegerToObject(endpointJson, 
                              "endpointNumber", 
                              addressTable[addressTableIndex].endpoints[epIndex]);
      cJSON_AddIntegerToObject(endpointJson, 
                              "deviceType", 
                              endpointList[endpointNum].deviceId);
      cJSON_AddItemToArray(nodeJsonEndpointArray, endpointJson);
    }
  }

  return nodeJson;
}

static void publishMqttRules(void) 
{
  char* topic = allocateAndFormMqttGatewayTopic("rules");
  uint16_t ruleIndex;
  cJSON* rulesJson;
  cJSON* rulesJsonRuleArray;
  cJSON* ruleJson;
  char* rulesJsonString;

  rulesJson = cJSON_CreateObject();
  rulesJsonRuleArray = cJSON_CreateArray();
  cJSON_AddItemToObject(rulesJson, "rules", rulesJsonRuleArray);
  for (ruleIndex = 0; ruleIndex < BIND_TABLE_SIZE; ruleIndex++) {
    if (bindTable[ruleIndex].inAddressTableEntry != 0xffff) {
        uint16_t inIndex = bindTable[ruleIndex].inAddressTableEntry;
        uint16_t outIndex = bindTable[ruleIndex].outAddressTableEntry;
        ruleJson = cJSON_CreateObject(); 
        cJSON_AddIntegerToObject(ruleJson, "inAddressTableIndex", inIndex);
        cJSON_AddIntegerToObject(ruleJson, "outAddressTableIndex", outIndex);
        cJSON_AddItemToArray(rulesJsonRuleArray, ruleJson);
    }
  }

  rulesJsonString = cJSON_PrintUnformatted(rulesJson);
  emberAfPluginTransportMqttPublish(topic, rulesJsonString);
  free(rulesJsonString);
  cJSON_Delete(rulesJson);
  free(topic);
}

static void publishMqttSettings(void)
{
  char* topic = allocateAndFormMqttGatewayTopic("settings");
  EmberNetworkParameters parameters;
  EmberNodeType nodeType;
  cJSON* settingsJson;
  char* settingsJsonString;
  char panIdString[PANID_STRING_LENGTH] = {0};
  char ncpStackVerString[HOSTVER_STRING_LENGTH] = {0};
  EmberVersion versionStruct;
  uint8_t ncpEzspProtocolVer;
  uint8_t ncpStackType;
  uint16_t ncpStackVer;
  uint8_t hostEzspProtocolVer = EZSP_PROTOCOL_VERSION;

  EmberStatus status = emberAfGetNetworkParameters(&nodeType, &parameters);
  sprintf(panIdString, "%04X", parameters.panId);

  ncpEzspProtocolVer = ezspVersion(hostEzspProtocolVer,
                                   &ncpStackType,
                                   &ncpStackVer);

  if (EZSP_SUCCESS == ezspGetVersionStruct(&versionStruct)) {
    sprintf(ncpStackVerString, "%d.%d.%d-%d", versionStruct.major, versionStruct.minor, versionStruct.patch,versionStruct.build);
  } else {
    sprintf(ncpStackVerString, "0x%2x",ncpStackVer);
  }

  settingsJson = cJSON_CreateObject();
  if (trafficReporting) {
    cJSON_AddTrueToObject(settingsJson, "trafficReporting");
  } else {
    cJSON_AddFalseToObject(settingsJson, "trafficReporting");
  }
  cJSON_AddStringToObject(settingsJson, "ncpVersion", ncpStackVerString);
  if (!emberAfNcpNeedsReset() && status == EMBER_SUCCESS) {
    cJSON_AddStringToObject(settingsJson, "networkState", "up");
    cJSON_AddStringToObject(settingsJson, "networkPanId", panIdString);
    cJSON_AddIntegerToObject(settingsJson, "radioTxPower", parameters.radioTxPower);
    cJSON_AddIntegerToObject(settingsJson, "radioChannel", parameters.radioChannel);
  } else {
    cJSON_AddStringToObject(settingsJson, "networkState", "down");
  }
  settingsJsonString = cJSON_PrintUnformatted(settingsJson);
  emberAfPluginTransportMqttPublish(topic, settingsJsonString);
  free(settingsJsonString);
  cJSON_Delete(settingsJson);
  free(topic);
}

static void publishMqttGatewayState(void)
{
  // Set an event to publish all the state updates so that they will be in scope
  // of the stack
  emberEventControlSetActive(stateUpdateEventControl);
}

void stateUpdateEventFunction(void)
{
  emberEventControlSetInactive(stateUpdateEventControl);
  publishMqttSettings();
  publishMqttRules();
  publishMqttDevices();
}

static void publishMqttContactSense(EmberNodeId nodeId,
                                    EmberEUI64 eui64,
                                    uint8_t contactState,
                                    uint8_t tamperState)
{
  char* topic = allocateAndFormMqttGatewayTopic("contactsense");
  char euiString[EUI64_STRING_LENGTH] = {0};
  char nodeIdString[NODEID_STRING_LENGTH] = {0};
  cJSON* contactSenseJson;
  char* contactSenseJsonString;
  char* dataString;
  eui64ToString(eui64, euiString);
  nodeIdToString(nodeId, nodeIdString);
  
  contactSenseJson = cJSON_CreateObject();
  dataString = createTwoByteHexString(ZCL_IAS_ZONE_CLUSTER_ID);
  cJSON_AddStringToObject(contactSenseJson, "clusterId", dataString);
    free(dataString);
  dataString = 
    createOneByteHexString(ZCL_ZONE_STATUS_CHANGE_NOTIFICATION_COMMAND_ID);
  cJSON_AddStringToObject(contactSenseJson, "commandId", dataString);
  free(dataString);
  cJSON_AddIntegerToObject(contactSenseJson, "contactState", contactState);
  cJSON_AddIntegerToObject(contactSenseJson, "tamperState", tamperState);
  cJSON_AddStringToObject(contactSenseJson, "nodeId", nodeIdString);
  cJSON_AddStringToObject(contactSenseJson, "nodeEui", euiString);
  contactSenseJsonString = cJSON_PrintUnformatted(contactSenseJson);
  emberAfPluginTransportMqttPublish(topic, contactSenseJsonString);
  free(contactSenseJsonString);
  cJSON_Delete(contactSenseJson);
  free(topic);
}

static void publishMqttZclContactSense(EmberNodeId nodeId,
                             EmberEUI64 eui64,
                             uint8_t contactState,
                             uint8_t tamperState)
{
  char* topic = allocateAndFormMqttGatewayTopic(ZCL_RESPONSE_TOPIC);
  char euiString[EUI64_STRING_LENGTH] = {0};
  char nodeIdString[NODEID_STRING_LENGTH] = {0};
  cJSON* contactSenseJson;
  char* contactSenseJsonString;
  char* dataString;

  eui64ToString(eui64, euiString);
  nodeIdToString(nodeId, nodeIdString);
  
  contactSenseJson = cJSON_CreateObject();
  dataString = createTwoByteHexString(ZCL_IAS_ZONE_CLUSTER_ID);
  cJSON_AddStringToObject(contactSenseJson, "clusterId", "0500");
  free(dataString);
  dataString = 
    createOneByteHexString(ZCL_ZONE_STATUS_CHANGE_NOTIFICATION_COMMAND_ID);
  cJSON_AddStringToObject(contactSenseJson, "commandId", dataString);
  free(dataString);
  cJSON_AddIntegerToObject(contactSenseJson, "sensorState", contactState);
  cJSON_AddIntegerToObject(contactSenseJson, "tamperState", tamperState);
  cJSON_AddStringToObject(contactSenseJson, "nodeId", nodeIdString);
  cJSON_AddStringToObject(contactSenseJson, "nodeEui", euiString);
  contactSenseJsonString = cJSON_PrintUnformatted(contactSenseJson);
  emberAfPluginTransportMqttPublish(topic, contactSenseJsonString);
  free(contactSenseJsonString);
  cJSON_Delete(contactSenseJson);
  free(topic);
}

static void publishMqttSwitchEvent(EmberNodeId nodeId,
                                    EmberEUI64 eui64,
                                    char* eventString)
{
  char* topic = allocateAndFormMqttGatewayTopic("switchevent");
  char euiString[EUI64_STRING_LENGTH] = {0};
  char nodeIdString[NODEID_STRING_LENGTH] = {0};
  cJSON* switchEventJson;
  char* switchEventJsonString;

  eui64ToString(eui64, euiString);
  nodeIdToString(nodeId, nodeIdString);
  switchEventJson = cJSON_CreateObject();
  cJSON_AddStringToObject(switchEventJson, "event", eventString);
  cJSON_AddStringToObject(switchEventJson, "nodeId", nodeIdString);
  cJSON_AddStringToObject(switchEventJson, "nodeEui", euiString);
  switchEventJsonString = cJSON_PrintUnformatted(switchEventJson);
  emberAfPluginTransportMqttPublish(topic, switchEventJsonString);
  free(switchEventJsonString);
  cJSON_Delete(switchEventJson);
  free(topic);
}

static void publishMqttDeviceStateChange(EmberNodeId nodeId,
                                  EmberEUI64 eui64,
                                  uint8_t state)
{
  char* topic = allocateAndFormMqttGatewayTopic("devicestatechange");
  char euiString[EUI64_STRING_LENGTH] = {0};
  char nodeIdString[NODEID_STRING_LENGTH] = {0};
  cJSON* stateChangeJson;
  char* stateChangeJsonString;

  eui64ToString(eui64, euiString);
  nodeIdToString(nodeId, nodeIdString);

  stateChangeJson = cJSON_CreateObject();
  cJSON_AddStringToObject(stateChangeJson, "nodeId", nodeIdString);
  cJSON_AddStringToObject(stateChangeJson, "nodeEui", euiString);
  cJSON_AddIntegerToObject(stateChangeJson, "deviceState", state);
  stateChangeJsonString = cJSON_PrintUnformatted(stateChangeJson);
  emberAfPluginTransportMqttPublish(topic, stateChangeJsonString);
  free(stateChangeJsonString);
  cJSON_Delete(stateChangeJson);
  free(topic);
}

static void publishMqttDeviceJoined(EmberEUI64 eui64)
{
  char* topic = allocateAndFormMqttGatewayTopic("devicejoined");
  uint16_t addressTableIndex;
  cJSON* nodeJson;
  char* nodeJsonString;

  addressTableIndex = getAddressIndexFromIeee(eui64);
  nodeJson = buildNodeJson(addressTable[addressTableIndex].nodeId);

  nodeJsonString = cJSON_PrintUnformatted(nodeJson);
  emberAfPluginTransportMqttPublish(topic, nodeJsonString);
  free(nodeJsonString);
  cJSON_Delete(nodeJson);
  free(topic);
}

static void publishMqttDeviceLeft(EmberEUI64 eui64)
{
  char* topic = allocateAndFormMqttGatewayTopic("deviceleft");
  char euiString[EUI64_STRING_LENGTH] = {0};
  cJSON* nodeLeftJson;
  char* nodeLeftJsonString;

  eui64ToString(eui64, euiString);
  
  nodeLeftJson = cJSON_CreateObject();
  cJSON_AddStringToObject(nodeLeftJson, "nodeEui", euiString);
  nodeLeftJsonString = cJSON_PrintUnformatted(nodeLeftJson);
  emberAfPluginTransportMqttPublish(topic, nodeLeftJsonString);
  free(nodeLeftJsonString);
  cJSON_Delete(nodeLeftJson);
  free(topic);
}

static void publishMqttTrafficTestResult(uint16_t numDefaultResponses,
                                         uint16_t numTxSendErrors,
                                         uint16_t numTotalMessages)
{
  char* topic = allocateAndFormMqttGatewayTopic("traffictestresult");
  cJSON* testResultsJson;
  char* testResultsJsonString;
  
  testResultsJson = cJSON_CreateObject();
  cJSON_AddIntegerToObject(testResultsJson,
                          "defaultResponses",
                          numDefaultResponses);
  cJSON_AddIntegerToObject(testResultsJson, "sendErrors", numTxSendErrors);
  cJSON_AddIntegerToObject(testResultsJson, "totalMessages", numTotalMessages);
  testResultsJsonString = cJSON_PrintUnformatted(testResultsJson);
  emberAfPluginTransportMqttPublish(topic, testResultsJsonString);
  free(testResultsJsonString);
  cJSON_Delete(testResultsJson);
  free(topic);
}

static void publishMqttNodeHeartBeat(EmberNodeId nodeId,
                                 EmberEUI64 eui64,
                                 EmberAfClusterId clusterId,
                                 uint8_t* buffer,
                                 uint16_t bufLen) 
{ 
  char* topic = allocateAndFormMqttGatewayTopic("nodeheartbeat");
  uint16_t bufferIndex;
  char euiString[EUI64_STRING_LENGTH] = {0};
  char nodeIdString[NODEID_STRING_LENGTH] = {0};
  cJSON* globalReadJson;
  char* globalReadJsonString;

  int8_t rssi = buffer[ATTRIBUTE_BUFFER_REPORT_DATA_START];
  eui64ToString(eui64, euiString);
  nodeIdToString(nodeId, nodeIdString);
  globalReadJson = cJSON_CreateObject();
  cJSON_AddIntegerToObject(globalReadJson, "rssi", rssi);
  cJSON_AddStringToObject(globalReadJson, "nodeId", nodeIdString);
  cJSON_AddStringToObject(globalReadJson, "nodeEui", euiString);
  globalReadJsonString = cJSON_PrintUnformatted(globalReadJson);
  emberAfPluginTransportMqttPublish(topic, globalReadJsonString);
  free(globalReadJsonString);
  cJSON_Delete(globalReadJson);
  free(topic);
}
static void publishMqttAttribute(EmberNodeId nodeId,
                                 EmberEUI64 eui64,
                                 EmberAfClusterId clusterId,
                                 uint8_t* buffer,
                                 uint16_t bufLen) 
{ 
  char* topic = allocateAndFormMqttGatewayTopic("attributeupdate");
  char* topicZcl = 
          allocateAndFormMqttGatewayTopic(ZCL_RESPONSE_TOPIC);
  uint16_t bufferIndex;
  char euiString[EUI64_STRING_LENGTH] = {0};
  char nodeIdString[NODEID_STRING_LENGTH] = {0};
  char clusterIdString[CLUSTERID_STRING_LENGTH] = {0};
  char attribString[5] = {0}; // 4 chars + null char
  char dataTypeString[3] = {0}; // 2 chars + null char
  char statusString[3] = {0}; // 2 chars + null char
  cJSON* globalReadJson;
  char* globalReadJsonString;
  uint16_t bufferStringLength = (2 * bufLen) + 1; // 2 chars per byte + null char
  char* bufferString = (char*)malloc(bufferStringLength);
  memset(bufferString, 0, bufferStringLength);

  // Print buffer data as a hex string, starting at the data start byte
  for (bufferIndex = ATTRIBUTE_BUFFER_DATA_START;
       bufferIndex < bufLen;
       bufferIndex++) {
    sprintf(&bufferString[2 * (bufferIndex - ATTRIBUTE_BUFFER_DATA_START)],
            "%02X",
            buffer[bufferIndex]);
  }

  eui64ToString(eui64, euiString);
  nodeIdToString(nodeId, nodeIdString);
  sprintf(clusterIdString, "%04X", clusterId);
  sprintf(attribString,
          "%02X%02X",
          buffer[ATTRIBUTE_BUFFER_CLUSTERID_HIGH_BITS],
          buffer[ATTRIBUTE_BUFFER_CLUSTERID_LOW_BITS]);
  sprintf(dataTypeString, "%02X", buffer[ATTRIBUTE_BUFFER_DATA_TYPE]);
  sprintf(statusString, "%02X", buffer[ATTRIBUTE_BUFFER_SUCCESS_CODE]);

  globalReadJson = cJSON_CreateObject();
  cJSON_AddStringToObject(globalReadJson, "clusterId", clusterIdString);
  cJSON_AddStringToObject(globalReadJson, "attributeId", attribString);
  cJSON_AddStringToObject(globalReadJson, "attributeBuffer", bufferString);
  cJSON_AddStringToObject(globalReadJson, "attributeDataType", dataTypeString);
  cJSON_AddStringToObject(globalReadJson, "nodeId", nodeIdString);
  cJSON_AddStringToObject(globalReadJson, "nodeEui", euiString);
  cJSON_AddStringToObject(globalReadJson, "returnStatus", statusString);
  globalReadJsonString = cJSON_PrintUnformatted(globalReadJson);
  emberAfPluginTransportMqttPublish(topic, globalReadJsonString);
  emberAfPluginTransportMqttPublish(topicZcl, globalReadJsonString);
  free(globalReadJsonString);
  free(bufferString);
  cJSON_Delete(globalReadJson);
  free(topic);
  free(topicZcl);
}

static void publishMqttAttributeReport(EmberNodeId nodeId,
                                 EmberEUI64 eui64,
                                 EmberAfClusterId clusterId,
                                 uint8_t* buffer,
                                 uint16_t bufLen) 
{ 
  char* topic = allocateAndFormMqttGatewayTopic("attributeupdate");
  char* topicZcl = 
          allocateAndFormMqttGatewayTopic(ZCL_RESPONSE_TOPIC);
  uint16_t bufferIndex;
  char euiString[EUI64_STRING_LENGTH] = {0};
  char nodeIdString[NODEID_STRING_LENGTH] = {0};
  char clusterIdString[CLUSTERID_STRING_LENGTH] = {0};
  char attribString[5] = {0}; // 4 chars + null char
  char dataTypeString[3] = {0}; // 2 chars + null char
  cJSON* globalReadJson;
  char* globalReadJsonString;
  uint16_t bufferStringLength = (2 * bufLen) + 1; // 2 chars per byte + null char
  char* bufferString = (char*)malloc(bufferStringLength);
  memset(bufferString, 0, bufferStringLength);

  // Print buffer data as a hex string, starting at the data start byte
  for (bufferIndex = ATTRIBUTE_BUFFER_REPORT_DATA_START;
       bufferIndex < bufLen;
       bufferIndex++) {
    sprintf(&bufferString[2 * (bufferIndex - ATTRIBUTE_BUFFER_REPORT_DATA_START)],
            "%02X",
            buffer[bufferIndex]);
  }

  eui64ToString(eui64, euiString);
  nodeIdToString(nodeId, nodeIdString);
  sprintf(clusterIdString, "%04X", clusterId);
  sprintf(attribString,
          "%02X%02X",
          buffer[ATTRIBUTE_BUFFER_CLUSTERID_HIGH_BITS],
          buffer[ATTRIBUTE_BUFFER_CLUSTERID_LOW_BITS]);
  sprintf(dataTypeString, "%02X", buffer[ATTRIBUTE_BUFFER_REPORT_DATA_TYPE]);

  globalReadJson = cJSON_CreateObject();
  cJSON_AddStringToObject(globalReadJson, "clusterId", clusterIdString);
  cJSON_AddStringToObject(globalReadJson, "attributeId", attribString);
  cJSON_AddStringToObject(globalReadJson, "attributeBuffer", bufferString);
  cJSON_AddStringToObject(globalReadJson, "attributeDataType", dataTypeString);
  cJSON_AddStringToObject(globalReadJson, "nodeId", nodeIdString);
  cJSON_AddStringToObject(globalReadJson, "nodeEui", euiString);
  globalReadJsonString = cJSON_PrintUnformatted(globalReadJson);
  emberAfPluginTransportMqttPublish(topic, globalReadJsonString);
  emberAfPluginTransportMqttPublish(topicZcl, globalReadJsonString);
  free(globalReadJsonString);
  free(bufferString);
  cJSON_Delete(globalReadJson);
  free(topic);
}

static void publishMqttApsStatus(EmberStatus status,
                                 EmberAfClusterId clusterId,
                                 uint8_t commandId)
{
  char* topic = allocateAndFormMqttGatewayTopic(APS_RESPONSE_TOPIC);
  cJSON* defaultResponseJson;
  char* defaultResponseString;
  char* stringData;

  defaultResponseJson = cJSON_CreateObject();

  cJSON_AddStringToObject(defaultResponseJson, "statusType", 
                          "apsAck");
  cJSON_AddIntegerToObject(defaultResponseJson, "status", status);
  
  stringData = createTwoByteHexString(clusterId);
  cJSON_AddStringToObject(defaultResponseJson, "clusterId", stringData);
  free(stringData);
  
  stringData = createOneByteHexString(commandId);
  cJSON_AddStringToObject(defaultResponseJson, "commandId", stringData);
  free(stringData);
  
  defaultResponseString = cJSON_PrintUnformatted(defaultResponseJson);
  emberAfPluginTransportMqttPublish(topic, defaultResponseString);
  free(defaultResponseString);
  cJSON_Delete(defaultResponseJson);
  free(topic);
}

static void publishMqttDefaultResponse(EmberStatus status,
                                       EmberAfClusterId clusterId,
                                       uint8_t commandId)
{
  char* topic = allocateAndFormMqttGatewayTopic(APS_RESPONSE_TOPIC);
  cJSON* defaultResponseJson;
  char* defaultResponseString;
  char* stringData;

  defaultResponseJson = cJSON_CreateObject();

  cJSON_AddStringToObject(defaultResponseJson, "statusType", 
                          "defaultResponse");
  cJSON_AddIntegerToObject(defaultResponseJson, "status", status);
  
  stringData = createTwoByteHexString(clusterId);
  cJSON_AddStringToObject(defaultResponseJson, "clusterId", stringData);
  free(stringData);
  
  stringData = createOneByteHexString(commandId);
  cJSON_AddStringToObject(defaultResponseJson, "commandId", stringData);
  free(stringData);
  
  defaultResponseString = cJSON_PrintUnformatted(defaultResponseJson);
  emberAfPluginTransportMqttPublish(topic, defaultResponseString);
  free(defaultResponseString);
  cJSON_Delete(defaultResponseJson);
  free(topic);
}

static void publishMqttTrafficReportEvent(char* messageType,
                                          EmberStatus* status,
                                          int8_t* lastHopRssi,
                                          uint8_t* lastHopLqi,
                                          uint8_t* zclSequenceNumber,
                                          int64u timeMS) 
{
  char* topic = allocateAndFormMqttGatewayTopic("trafficreportevent");
  cJSON* trafficReportJson;
  char* trafficReportJsonString;
  char timeString[20] = {0}; // 20 character maximum for in64u, including null
  
  sprintf(timeString, "%llu", timeMS);

  trafficReportJson = cJSON_CreateObject();
  cJSON_AddStringToObject(trafficReportJson, "messageType", messageType);
  if (status) {
    cJSON_AddIntegerToObject(trafficReportJson, "returnStatus", *status);
  }
  if (lastHopRssi) {
    cJSON_AddIntegerToObject(trafficReportJson, "rssi", *lastHopRssi);
  }
  if (lastHopLqi) {
    cJSON_AddIntegerToObject(trafficReportJson, "linkQuality", *lastHopLqi);
  }
  if (zclSequenceNumber) {
    cJSON_AddIntegerToObject(trafficReportJson, "sequenceNumber", *zclSequenceNumber);
  }
  cJSON_AddStringToObject(trafficReportJson, "currentTimeMs", timeString);
  trafficReportJsonString = cJSON_PrintUnformatted(trafficReportJson);
  emberAfPluginTransportMqttPublish(topic, trafficReportJsonString);
  free(trafficReportJsonString);
  cJSON_Delete(trafficReportJson);
  free(topic);
}

static void publishMqttOtaEvent(char* messageType,
                                EmberNodeId nodeId,
                                EmberEUI64 eui64,
                                uint8_t* status,
                                int32u* blocksSent,
                                uint8_t* blockSize,
                                uint16_t* manufacturerId,
                                uint16_t* imageTypeId,
                                int32u* firmwareVersion)
{
  char* topic = allocateAndFormMqttGatewayTopic("otaevent");
  char euiString[EUI64_STRING_LENGTH] = {0};
  char nodeIdString[NODEID_STRING_LENGTH] = {0};
  char manufacturerIdString[5] = {0}; // 4 chars + null char
  char imageTypeIdString[5] = {0}; // 4 chars + null char
  char firmwareVersionString[9] = {0}; // 8 chars + null char
  cJSON* otaJson;
  char* otaJsonString;

  eui64ToString(eui64, euiString);
  nodeIdToString(nodeId, nodeIdString);

  otaJson = cJSON_CreateObject();
  cJSON_AddStringToObject(otaJson, "messageType", messageType);
  cJSON_AddStringToObject(otaJson, "nodeId", nodeIdString);
  cJSON_AddStringToObject(otaJson, "nodeEui", euiString);
  if (status) {
    cJSON_AddIntegerToObject(otaJson, "returnStatus", *status);
  }
  if (blocksSent) {
    cJSON_AddIntegerToObject(otaJson, "blocksSent", *blocksSent);
  }
  if (blockSize) {
    cJSON_AddIntegerToObject(otaJson, "blockSize", *blockSize);
  } 
  if (manufacturerId) {
    sprintf(manufacturerIdString, "%04X", *manufacturerId);
    cJSON_AddStringToObject(otaJson, "manufacturerId", manufacturerIdString);
  }
  if (imageTypeId) {
    sprintf(imageTypeIdString, "%04X", *imageTypeId);
    cJSON_AddStringToObject(otaJson, "imageTypeId", imageTypeIdString);
  }
  if (firmwareVersion) {
    sprintf(firmwareVersionString, "%08X", *firmwareVersion);
    cJSON_AddStringToObject(otaJson, "firmwareVersion", firmwareVersionString);
  }
  otaJsonString = cJSON_PrintUnformatted(otaJson);
  emberAfPluginTransportMqttPublish(topic, otaJsonString);
  free(otaJsonString);
  cJSON_Delete(otaJson);
  free(topic);
}

static void publishMqttCommandExecuted(char* cliCommand)
{
  char* topic = allocateAndFormMqttGatewayTopic("executed");
  cJSON* executedJson;
  char* executedJsonString;

  executedJson = cJSON_CreateObject();
  cJSON_AddStringToObject(executedJson, "command", cliCommand);
  executedJsonString = cJSON_PrintUnformatted(executedJson);
  emberAfPluginTransportMqttPublish(topic, executedJsonString);
  free(executedJsonString);
  cJSON_Delete(executedJson);
  free(topic);
}

static void publishMqttDelayExecuted(int32u postDelayMs)
{
  char* topic = allocateAndFormMqttGatewayTopic("executed");
  cJSON* executedJson;
  char* executedJsonString;

  executedJson = cJSON_CreateObject();
  cJSON_AddIntegerToObject(executedJson, "delay", postDelayMs);
  executedJsonString = cJSON_PrintUnformatted(executedJson);
  emberAfPluginTransportMqttPublish(topic, executedJsonString);
  free(executedJsonString);
  cJSON_Delete(executedJson);
  free(topic);
}

// IAS Zone Callabacks
void emberAfPluginIASZoneClientContactCallback(uint8_t zoneStatus) 
{
  EmberEUI64 nodeEui64;
  EmberNodeId nodeId = emberAfCurrentCommand()->source;
  uint8_t contactState = zoneStatus & BIT(IAS_ZONE_CONTACT_STATE_BIT);
  uint8_t tamperState = (zoneStatus & BIT(IAS_ZONE_TAMPER_STATE_BIT)) 
                        >> IAS_ZONE_TAMPER_STATE_BIT;
  getIeeeFromNodeId(nodeId, nodeEui64);
  publishMqttContactSense(nodeId, nodeEui64, contactState, tamperState);
}

// OTA Callbacks
void emberAfPluginOtaServerFinishedCallback(uint16_t manufacturerId,
                                            uint16_t imageTypeId,
                                            int32u firmwareVersion,
                                            EmberNodeId nodeId,
                                            uint8_t status)
{
  EmberEUI64 nodeEui64;
  getIeeeFromNodeId(nodeId, nodeEui64);
  publishMqttOtaEvent("otaFinished",
                      nodeId,
                      nodeEui64,
                      &status,
                      NULL, // blockSent is unused
                      NULL, // actualLength is unused
                      &manufacturerId,
                      &imageTypeId,
                      &firmwareVersion);
}

void emberAfPluginOtaServerBlockSentCallback(uint8_t actualLength,
                                             uint16_t manufacturerId,
                                             uint16_t imageTypeId,
                                             int32u firmwareVersion)
{
  // Use a throttle value here to control the amount of updates being published
  if (otaBlockSent % BLOCK_SENT_THROTTLE_VALUE == 0) {
    EmberNodeId nodeId = emberAfCurrentCommand()->source;
    EmberEUI64 nodeEui64;
    getIeeeFromNodeId(nodeId, nodeEui64);
    publishMqttOtaEvent("otaBlockSent",
                        nodeId,
                        nodeEui64,
                        NULL, // status is unused
                        &otaBlockSent,
                        &actualLength,
                        &manufacturerId,
                        &imageTypeId,
                        &firmwareVersion);
  }

  otaBlockSent++;
}

void emberAfPluginOtaServerUpdateStartedCallback(uint16_t manufacturerId,
                                                 uint16_t imageTypeId,
                                                 int32u firmwareVersion,
                                                 uint8_t maxDataSize,
                                                 int32u offset) 
{
  otaBlockSent = 0; // Note that this global block sent count only supports 1 OTA
  EmberNodeId nodeId = emberAfCurrentCommand()->source;
  EmberEUI64 nodeEui64;
  getIeeeFromNodeId(nodeId, nodeEui64);
  publishMqttOtaEvent("otaStarted",
                      nodeId,
                      nodeEui64,
                      NULL, // status is unused
                      NULL, // blockSent is unused
                      NULL, // actualLength is unused
                      &manufacturerId,
                      &imageTypeId,
                      &firmwareVersion);
}

void emberAfPluginOtaServerImageFailedCallback(void) 
{
  otaBlockSent = 0; // Note that this global block sent count only supports 1 OTA
  EmberNodeId nodeId = emberAfCurrentCommand()->source;
  EmberEUI64 nodeEui64;
  getIeeeFromNodeId(nodeId, nodeEui64);
  publishMqttOtaEvent("otaFailed",
                      nodeId,
                      nodeEui64,
                      NULL, // status is unused
                      NULL, // blockSent is unused
                      NULL, // actualLength is unused
                      NULL, // manufacturerId is unused
                      NULL, // imageTypeId is unused
                      NULL); // firmwareVersion is unused
}

// Non-cluster Callbacks
bool emberAfMessageSentCallback(EmberOutgoingMessageType type,
                                uint16_t indexOrDestination,
                                EmberApsFrame* apsFrame,
                                uint16_t msgLen,
                                uint8_t* message,
                                EmberStatus status)
{
  if (trafficReporting) {
    // This specifically uses the emberAfIncomingZclSequenceNumber instead
    // of the adjusted sequence number used in emberAfPreMessageReceivedCallback
    // and emberAfPreMessageSendCallback
    publishMqttTrafficReportEvent("messageSent",
                                  &status,
                                  NULL, // rssi unused
                                  NULL, // lqi unused
                                  NULL,
                                  getTimeMS());
  }
  
  publishMqttApsStatus(status, apsFrame->clusterId, message[COMMAND_OFFSET]);

#if defined(EMBER_AF_PLUGIN_TRAFFIC_TEST)
  emAfTrafficTestTrackTxErrors(status);
#endif

  // track the state of the device.
  emberAfPluginDeviceTableMessageSentStatus(indexOrDestination, status);

  if (status != EMBER_SUCCESS) {
    emberSerialPrintfLine(APP_SERIAL,
                          "%2x failed with code %x", 
                          indexOrDestination, 
                          status);

    if (indexOrDestination > EMBER_ZLL_MAX_NODE_ID) {
      return false;
    }
    initiateRouteRepair(indexOrDestination);
  }
  return false;
}

bool emberAfPreMessageReceivedCallback(EmberAfIncomingMessage* incomingMessage)
{
  if (trafficReporting) {
    publishMqttTrafficReportEvent("preMessageReceived",
                                  NULL, // status unsused
                                  &(incomingMessage->lastHopRssi),
                                  &(incomingMessage->lastHopLqi),
                                  NULL,
                                  getTimeMS());
  }
  return false;
}

bool emberAfPreMessageSendCallback(EmberAfMessageStruct* messageStruct,
                                      EmberStatus* status)
{
  if (trafficReporting) {
    publishMqttTrafficReportEvent("preMessageSend",
                                  NULL, // status unsused
                                  NULL, // rssi unused
                                  NULL, // lqi unused
                                  NULL,
                                  getTimeMS());
  }
  return false;
}

bool emberAfReadAttributesResponseCallback(EmberAfClusterId clusterId,
                                              uint8_t* buffer,
                                              uint16_t bufLen)
{
  EmberEUI64 nodeEui64;
  EmberNodeId nodeId = emberAfCurrentCommand()->source;
  getIeeeFromNodeId(nodeId, nodeEui64);

  // If a zero-length attribute is reported, just leave
  if (bufLen == 0) {
    emberSerialPrintfLine(APP_SERIAL, "Read attributes callback: zero length buffer");
    return false;
  }

  emberSerialPrintfLine(APP_SERIAL, "Read attributes: 0x%2x", clusterId);
  publishMqttAttribute(nodeId,
                       nodeEui64,
                       clusterId,
                       buffer,
                       bufLen);
  printAttributeBuffer(clusterId, buffer, bufLen);

  return false;
}

bool emberAfReportAttributesCallback(EmberAfClusterId clusterId,
                                        uint8_t * buffer,
                                        uint16_t bufLen)
{
  EmberEUI64 nodeEui64;
  EmberNodeId nodeId = emberAfCurrentCommand()->source;
  getIeeeFromNodeId(nodeId, nodeEui64);

  if (bufLen == 0) {
    emberSerialPrintfLine(APP_SERIAL, "Report attributes callback: zero length buffer");
    return false;
  }

  uint8_t i;
  uint8_t* bufferPtr = buffer;
  
  /* Buffer [0] is low bits, Buffer [1] is high bits, Buffer [2] is dataType, Buffer [3+] is data */
  emberSerialPrintfLine(APP_SERIAL, "Reporting attributes for cluster: 0x%2x", clusterId);
  for (i = 0; i < bufLen;) {
    /* Get Length of Attribute Buffer */
    uint8_t bufferDataSizeI = emberAfGetDataSize(bufferPtr[ATTRIBUTE_BUFFER_REPORT_DATA_TYPE]);
    uint8_t bufferSizeI = bufferDataSizeI + 3;

    //Copy buffer to attributeBufferI
    uint8_t* bufferTemp = (uint8_t*)malloc(bufferSizeI);
    memcpy(bufferTemp, bufferPtr, bufferSizeI);

    /* Set i to point to [attrLSB,attrMSB,dataT,buffer,nAttrLSB,nAttrMSB,nextDataT,nextBuffer] */
    /*                   [       ,       ,     , ..n  ,   X    ,        ,         ,  ..n     ] */
    bufferPtr = bufferPtr + bufferSizeI;
    i = i + bufferSizeI;

    emberSerialPrintfLine(APP_SERIAL, "Reported attribute: 0x%02X%02X, Type: %02X",
          bufferTemp[ATTRIBUTE_BUFFER_CLUSTERID_HIGH_BITS],
          bufferTemp[ATTRIBUTE_BUFFER_CLUSTERID_LOW_BITS],
          bufferTemp[ATTRIBUTE_BUFFER_REPORT_DATA_TYPE]);

    emberAfRulesEngineReportAttributeRespones(clusterId, bufferTemp, bufferSizeI);
    if(clusterId == ZCL_DIAGNOSTICS_CLUSTER_ID)
    {
        publishMqttNodeHeartBeat(nodeId,
                                 nodeEui64,
                                 clusterId,
                                 bufferTemp,
                                 bufferSizeI);
    }
    else
    {
        publishMqttAttributeReport(nodeId,
                               nodeEui64,
                               clusterId,
                               bufferTemp,
                               bufferSizeI);
    }
    free(bufferTemp);
  }

  return false;
}

// Device Table Callbacks
void emberAfPluginDeviceTableNewDeviceCallback(EmberEUI64 nodeEui64) 
{
  publishMqttDeviceJoined(nodeEui64);
}

void emberAfPluginDeviceTableDeviceLeftCallback(EmberEUI64 nodeEui64) 
{
  publishMqttDeviceLeft(nodeEui64);
}

void emberAfPluginDeviceTableRejoinDeviceCallback(EmberEUI64 nodeEui64) 
{
  publishMqttDeviceJoined(nodeEui64);
}

void emberAfPluginDeviceTableStateChangeCallback(EmberNodeId nodeId, uint8_t state)
{
  EmberEUI64 nodeEui64;
  getIeeeFromNodeId(nodeId, nodeEui64);
  publishMqttDeviceStateChange(nodeId, nodeEui64, state);
}

void emberAfPluginDeviceTableClearedCallback(void)
{
  publishMqttGatewayState();
}

// Rules Engine Callbacks
void emberAfPluginRulesEngineChangedCallback(void)
{
  publishMqttRules();
}

// MQTT Transport Callbacks
void emberAfPluginTransportMqttConnectedCallback(void)
{
  LinkListElement* currentElement = NULL;
  MqttTopicHandlerMap* topicHandlerMap = NULL;

  emberSerialPrintfLine(APP_SERIAL, 
                        "MQTT connected, starting gateway heartbeat and command processing");
  emberEventControlSetActive(heartbeatEventControl);
  emberEventControlSetActive(processCommandEventControl);
  
  // Loop through the Topic Handler Map to subscribe to all the topics
  do {
    currentElement = linkListNextElement(topicHandlerList, &currentElement);
    if (currentElement != NULL) {
      topicHandlerMap = (MqttTopicHandlerMap*)currentElement->content;
      emberAfPluginTransportMqttSubscribe(topicHandlerMap->topic);
    }
  } while (currentElement != NULL);

  // Since we are newly connecting, dump our complete device state and rules list
  publishMqttGatewayState();
}

void emberAfPluginTransportMqttDisconnectedCallback(void)
{
  emberSerialPrintfLine(APP_SERIAL, "MQTT disconnected, stopping gateway heartbeat");
  emberEventControlSetInactive(heartbeatEventControl);
  emberEventControlSetInactive(processCommandEventControl);
}

bool emberAfPluginTransportMqttMessageArrivedCallback(const char* topic,
                                                         const char* payload)
{
  cJSON* incomingMessageJson;
  LinkListElement* currentElement = NULL;
  MqttTopicHandlerMap* topicHandlerMap = NULL;

  incomingMessageJson = cJSON_Parse(payload);
 
  // Loop through the Topic Handler Map to determine which handler to call
  do {
    currentElement = linkListNextElement(topicHandlerList, &currentElement);
    if (currentElement != NULL) {
      topicHandlerMap = (MqttTopicHandlerMap*)currentElement->content;

      // If the incoming topic matches a topic in the map, call it's handler
      if (strcmp(topic, topicHandlerMap->topic) == 0) {
        topicHandlerMap->topicHandler(incomingMessageJson);
        break;
      }
    }
  } while (currentElement != NULL);

  cJSON_Delete(incomingMessageJson);

  // Return true, this tells the MQTT client we have handled the incoming message
  return true;
}

static void handleCommandsMessage(cJSON* messageJson)
{
  uint8_t commandIndex;
  cJSON* commandsJson;
  cJSON* commandJson;
  cJSON* commandStringJson;
  cJSON* postDelayMsJson;

  if (messageJson != NULL) {
    emberSerialPrintfLine(APP_SERIAL,
                          "Handling Commands Message: %s",
                          cJSON_PrintUnformatted(messageJson));
    commandsJson = cJSON_GetObjectItem(messageJson, "commands");
    if (commandsJson != NULL) {
      for (commandIndex = 0;
           commandIndex < cJSON_GetArraySize(commandsJson);
           commandIndex++)
      {
        commandJson = cJSON_GetArrayItem(commandsJson, commandIndex);
        if (commandJson != NULL) {
          commandStringJson = cJSON_GetObjectItem(commandJson, "command");
          if (commandStringJson != NULL) {
            addCliCommandToList(commandList, commandStringJson->valuestring);
          }

          postDelayMsJson = cJSON_GetObjectItem(commandJson, "postDelayMs");
          if (postDelayMsJson != NULL) {
            addPostDelayMsToList(commandList, (int32u)postDelayMsJson->valueint);
          }
        }
      }
    }
  }
}

static void addCliCommandToList(LinkList* list, const char* cliCommandString)
{
  GatewayCommand* gatewayCommand = allocateGatewayCommand();
  char* cliCommandStringForList =
    (char*)malloc(strlen(cliCommandString) + 1); // Add NULL char
  strcpy(cliCommandStringForList, cliCommandString); // Copies string including NULL char
  
  gatewayCommand->commandType = COMMAND_TYPE_CLI;
  gatewayCommand->cliCommand = cliCommandStringForList;

  linkListPushBack(list, (void*)gatewayCommand);
}

static void addPostDelayMsToList(LinkList* list, int32u postDelayMs)
{
  GatewayCommand* gatewayCommand = allocateGatewayCommand();

  gatewayCommand->commandType = COMMAND_TYPE_POST_DELAY;
  gatewayCommand->postDelayMs = postDelayMs;

  linkListPushBack(list, (void*)gatewayCommand);
}

void processCommandEventFunction(void)
{
  emberEventControlSetDelayMS(processCommandEventControl, PROCESS_COMMAND_RATE_MS);
  LinkListElement* commandListItem = NULL;
  GatewayCommand* gatewayCommand;
  
  assert(commandList != NULL);

  // Get the head of the command list
  commandListItem = linkListNextElement(commandList, &commandListItem);

  // If there is nothing there, continue on
  if (commandListItem == NULL) {
    return;
  }

  gatewayCommand = commandListItem->content;
  assert(gatewayCommand != NULL);

  // CLI command processing
  if (gatewayCommand->commandType == COMMAND_TYPE_CLI) {
    // Process our command string, then pop the command from the list
    // First send the CLI, then send a /n to simulate the "return" key
    emberProcessCommandString((uint8_t*)gatewayCommand->cliCommand,
                              strlen(gatewayCommand->cliCommand));
    emberProcessCommandString((uint8_t*)"\n",
                              strlen("\n"));
    publishMqttCommandExecuted(gatewayCommand->cliCommand);
    emberSerialPrintfLine(APP_SERIAL,
                          "CLI command executed: %s",
                          gatewayCommand->cliCommand);
    freeGatewayCommand(gatewayCommand);
    linkListPopFront(commandList);
  }

  // Delay processing
  if (gatewayCommand->commandType == COMMAND_TYPE_POST_DELAY) {
    // If our resume time hasn't been initialized we are starting the delay
    if (gatewayCommand->resumeTime == 0) {
      // Make sure delay isn't 0, if so pop the list and move on
      if (gatewayCommand->postDelayMs == 0) {
        freeGatewayCommand(gatewayCommand);
        linkListPopFront(commandList);
      }
      // Calculate the time to resume
      gatewayCommand->resumeTime = getTimeMS() + gatewayCommand->postDelayMs;
    } else {
      // If we are already delaying, see if it's time to resume
      if (getTimeMS() > gatewayCommand->resumeTime) {
        // Resume by popping this delay from the list
        publishMqttDelayExecuted(gatewayCommand->postDelayMs);
        emberSerialPrintfLine(APP_SERIAL, 
                              "Delay executed for: %d ms",
                              gatewayCommand->postDelayMs);
        freeGatewayCommand(gatewayCommand);
        linkListPopFront(commandList);
      }
    }
  }
}

static GatewayCommand* allocateGatewayCommand()
{
  GatewayCommand* gatewayCommand = (GatewayCommand*)malloc(sizeof(GatewayCommand));
  gatewayCommand->commandType = 0;
  gatewayCommand->cliCommand = NULL;
  gatewayCommand->resumeTime = 0;
  gatewayCommand->postDelayMs = 0;
  return gatewayCommand;
}

static void freeGatewayCommand(GatewayCommand* gatewayCommand)
{
  if (gatewayCommand != NULL) {
    if (gatewayCommand->cliCommand != NULL) {
      free(gatewayCommand->cliCommand);
    }
    free(gatewayCommand);
  }
}

static void handlePublishStateMessage(cJSON* messageJson)
{
  emberSerialPrintfLine(APP_SERIAL, "Handling Publish State Message");
  publishMqttGatewayState();
}

static void handleUpdateSettingsMessage(cJSON* messageJson)
{
  cJSON* trafficReportingJson;
  if (messageJson != NULL) {
    emberSerialPrintfLine(APP_SERIAL,
                          "Handling Update Settings Message: %s",
                          cJSON_PrintUnformatted(messageJson));
    trafficReportingJson = cJSON_GetObjectItem(messageJson, "trafficReporting");

    if (trafficReportingJson != NULL) {
      if (trafficReportingJson->valueint == 1) {
        trafficReporting = true;
      } else if (trafficReportingJson->valueint == 0) {
        trafficReporting = false;
      }
    }
  }
}

// Traffic Test Callbacks
void emberAfPluginTrafficTestReportResultsCallback(uint16_t numDefaultResponses,
                                                    uint16_t numTxSendErrors,
                                                    uint16_t numTotalMessages)
{
  publishMqttTrafficTestResult(numDefaultResponses, numTxSendErrors, numTotalMessages);
}

void emberAfPluginTrafficTestDefaultResponseCallback(EmberStatus status,
                                                     EmberAfClusterId clusterId,
                                                     uint8_t commandId)
{
  if (trafficReporting) {
    uint8_t sequenceNumber = emberAfIncomingZclSequenceNumber;
    publishMqttTrafficReportEvent("defaultResponse",
                                  &status,
                                  NULL, // rssi unused
                                  NULL, // lqi unused
                                  &sequenceNumber,
                                  getTimeMS());
  }
  
  publishMqttDefaultResponse(status, clusterId, commandId);
}

void emberAfPluginTrafficTestMessageBuiltCallback()
{
  if (trafficReporting) {
    uint8_t sequenceNumber = emberAfGetLastSequenceNumber();
    publishMqttTrafficReportEvent("messageBuilt",
                                  NULL, // status unused
                                  NULL, // rssi unused
                                  NULL, // lqi unused
                                  &sequenceNumber,
                                  getTimeMS());
  }
}

// String/other helpers
static void eui64ToString(EmberEUI64 eui, char* euiString)
{
  sprintf(euiString, "%02X%02X%02X%02X%02X%02X%02X%02X",
          eui[7],
          eui[6],
          eui[5],
          eui[4],
          eui[3],
          eui[2],
          eui[1],
          eui[0]);
}

static void nodeIdToString(EmberNodeId nodeId, char* nodeIdString)
{
  sprintf(nodeIdString, "0x%04X", nodeId);
}

static int64u getTimeMS(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  int64u timeMs = (int64u)(tv.tv_sec) * 1000 + (int64u)(tv.tv_usec) / 1000;
  return timeMs;
}

static void printAttributeBuffer(uint16_t clusterId, 
                                 uint8_t* buffer, 
                                 uint16_t bufLen)
{
  uint16_t bufferIndex;

  emberSerialPrintfLine(APP_SERIAL, 
                        " Cluster, Attribute: %04X, %02X%02X"
                        " Success Code: %02X"
                        " Data Type: %02X\n"
                        " Hex Buffer: ",
                        clusterId,
                        buffer[ATTRIBUTE_BUFFER_CLUSTERID_HIGH_BITS],
                        buffer[ATTRIBUTE_BUFFER_CLUSTERID_LOW_BITS],
                        buffer[ATTRIBUTE_BUFFER_SUCCESS_CODE],
                        buffer[ATTRIBUTE_BUFFER_DATA_TYPE]);

  // Print buffer data as a hex string, starting at the data start byte
  for (bufferIndex = ATTRIBUTE_BUFFER_DATA_START;
       bufferIndex < bufLen;
       bufferIndex++) {
    emberSerialPrintf(APP_SERIAL, "%02X", buffer[bufferIndex]);
  }
  emberSerialPrintfLine(APP_SERIAL, "");
}

/** @brief Configure Reporting Response
 *
 * This function is called by the application framework when a Configure
 * Reporting Response command is received from an external device.  The
 * application should return true if the message was processed or false if it
 * was not.
 *
 * @param clusterId The cluster identifier of this response.  Ver.: always
 * @param buffer Buffer containing the list of attribute status records.  Ver.:
 * always
 * @param bufLen The length in bytes of the list.  Ver.: always
 */
boolean emberAfConfigureReportingResponseCallback(EmberAfClusterId clusterId,
                                                  int8u *buffer,
                                                  int16u bufLen)
{
  char* topic = allocateAndFormMqttGatewayTopic(ZDO_RESPONSE_TOPIC);
  cJSON* configureReportResponseJson;
  char* configureReportResponseString;
  char* dataString;

  configureReportResponseJson = cJSON_CreateObject();
  
  cJSON_AddStringToObject(configureReportResponseJson, 
                          "zdoType", 
                          "configureReportResponse");

  dataString = createOneByteHexString(buffer[0]);
  cJSON_AddStringToObject(configureReportResponseJson, 
                       "status", 
                       dataString);
  free(dataString);
  
  configureReportResponseString = 
    cJSON_PrintUnformatted(configureReportResponseJson);
  emberAfPluginTransportMqttPublish(topic, configureReportResponseString);
  free(configureReportResponseString);
  cJSON_Delete(configureReportResponseJson);
  free(topic);
  
  return false;
}

/** @brief Read Reporting Configuration Response
 *
 * This function is called by the application framework when a Read Reporting
 * Configuration Response command is received from an external device.  The
 * application should return true if the message was processed or false if it
 * was not.
 *
 * @param clusterId The cluster identifier of this response.  Ver.: always
 * @param buffer Buffer containing the list of attribute reporting configuration
 * records.  Ver.: always
 * @param bufLen The length in bytes of the list.  Ver.: always
 */
boolean emberAfReadReportingConfigurationResponseCallback(
          EmberAfClusterId clusterId,
          int8u *buffer,
          int16u bufLen)
{
  char* topic = allocateAndFormMqttGatewayTopic(ZDO_RESPONSE_TOPIC);
  cJSON* reportTableJson;
  char* reportTableString;
  char* tempString;
  char* dataString = (char*)malloc(2*(bufLen));
  uint8_t i;
  
  for (i=0; i < (2*bufLen); i++) {
    dataString[i] = 0;
  }
  
  reportTableJson = cJSON_CreateObject();
  
  cJSON_AddStringToObject(reportTableJson, 
                          "zdoType", 
                          "reportTableEntry");
  
  tempString = createOneByteHexString(buffer[READ_REPORT_CONFIG_STATUS]);
  cJSON_AddStringToObject(reportTableJson, "status", tempString);
  free(tempString);
  
  cJSON_AddIntegerToObject(reportTableJson, 
                           "direction", 
                           buffer[READ_REPORT_CONFIG_DIRECTION]);
  
  tempString = createTwoByteHexString(clusterId);
  cJSON_AddStringToObject(reportTableJson, "clusterId", tempString);
  free(tempString);
                           
  tempString = createTwoByteHexString(
    HIGH_LOW_TO_INT(buffer[READ_REPORT_CONFIG_ATTRIBUTE_ID+1], 
                    buffer[READ_REPORT_CONFIG_ATTRIBUTE_ID]));
  cJSON_AddStringToObject(reportTableJson, "attributeId", tempString);
  free(tempString);
  
  tempString = createOneByteHexString(buffer[READ_REPORT_CONFIG_DATA_TYPE]);
  cJSON_AddStringToObject(reportTableJson, "dataType", tempString);
  free(tempString);
  
  tempString = createTwoByteHexString(
    HIGH_LOW_TO_INT(buffer[READ_REPORT_CONFIG_MIN_INTERVAL+1], 
                    buffer[READ_REPORT_CONFIG_MIN_INTERVAL]));
  cJSON_AddStringToObject(reportTableJson, "minInterval", tempString);
  free(tempString);
  
  tempString = createTwoByteHexString(
    HIGH_LOW_TO_INT(buffer[READ_REPORT_CONFIG_MAX_INTERVAL+1], 
                    buffer[READ_REPORT_CONFIG_MAX_INTERVAL]));
  cJSON_AddStringToObject(reportTableJson, "maxInterval", tempString);
  free(tempString);
  
  for (i = READ_REPORT_CONFIG_DATA; i < bufLen; i++) {
    sprintf(& (dataString[2*(i-READ_REPORT_CONFIG_DATA)]), "%02X", buffer[i]);
  }
  cJSON_AddStringToObject(reportTableJson, "data", dataString);
  
  reportTableString = cJSON_PrintUnformatted(reportTableJson);
  emberAfPluginTransportMqttPublish(topic, reportTableString);
  free(reportTableString);
  cJSON_Delete(reportTableJson);
  free(topic);
  free(dataString);
  
  return false;
}

/** @brief BindResponse
 *
 * This callback is called when a device received a bind request.
 *
 * @param nodeId   Ver.: always
 * @param apsFrame   Ver.: always
 * @param message   Ver.: always
 * @param length   Ver.: always
 */
void emberAfPluginDeviceTableBindResponseCallback(EmberNodeId nodeId,
                                                  EmberApsFrame* apsFrame,
                                                  uint8_t* message,
                                                  uint16_t length)
{
  char* topic = allocateAndFormMqttGatewayTopic(ZDO_RESPONSE_TOPIC);
  cJSON* objectJson;
  char* objectJsonString;
  char* dataString;

  objectJson = cJSON_CreateObject();
  
  cJSON_AddStringToObject(objectJson, "zdoType", "bindResponse");

  dataString = 
    createOneByteHexString(message[DEVICE_TABLE_BIND_RESPONSE_STATUS]);
  cJSON_AddStringToObject(objectJson, "status", dataString);
  free(dataString);
  
  objectJsonString = 
    cJSON_PrintUnformatted(objectJson);
  emberAfPluginTransportMqttPublish(topic, objectJsonString);
  free(objectJsonString);
  cJSON_Delete(objectJson);
  free(topic);
}

/** @brief BindingTableResponse
 *
 * This callback is called when a device received a binding table request.
 *
 * @param nodeId   Ver.: always
 * @param apsFrame   Ver.: always
 * @param message   Ver.: always
 * @param length   Ver.: always
 */
void emberAfPluginDeviceTableBindingTableResponseCallback(
  EmberNodeId nodeId,
  EmberApsFrame* apsFrame,
  uint8_t* message,
  uint16_t length)
{
  char* topic = allocateAndFormMqttGatewayTopic(ZDO_RESPONSE_TOPIC);
  cJSON* objectJson;
  cJSON* entryArrayJson;
  cJSON* tableEntryJson;
  char* objectJsonString;
  uint8_t* messagePointer;
  uint8_t numEntries, entryCounter; 
  char* euiString[EUI64_STRING_LENGTH];
  char* dataString;
  
  numEntries = message[BINDING_TABLE_RESPONSE_NUM_ENTRIES]; // list count

  objectJson = cJSON_CreateObject();
  entryArrayJson = cJSON_CreateArray();
  
  cJSON_AddStringToObject(objectJson, "zdoType", "bindTableResponse");
  
  dataString = createOneByteHexString(message[BINDING_TABLE_RESPONSE_STATUS]);
  cJSON_AddStringToObject(objectJson, "status", dataString);
  free(dataString);
  
  messagePointer = message + BINDING_TABLE_RESPONSE_ENTRIES;
  
  for (entryCounter = 0; entryCounter < numEntries; entryCounter++) {
    tableEntryJson = cJSON_CreateObject();
    
    eui64ToString(&(messagePointer[BINDING_ENTRY_EUI]),euiString);
    cJSON_AddStringToObject(tableEntryJson, "sourceEui", euiString);
    
    cJSON_AddIntegerToObject(tableEntryJson, 
                             "sourceEndpoint", 
                             messagePointer[BINDING_ENTRY_SOURCE_ENDPOINT]);
    cJSON_AddIntegerToObject(tableEntryJson, 
                             "addressMode", 
                             messagePointer[BINDING_ENTRY_ADDRESS_MODE]);

    dataString = 
      createTwoByteHexString(
        HIGH_LOW_TO_INT(messagePointer[BINDING_ENTRY_CLUSTER_ID+1],
                        messagePointer[BINDING_ENTRY_CLUSTER_ID]));
    cJSON_AddStringToObject(tableEntryJson, "clusterId", dataString);
    free(dataString);
    
    eui64ToString(&(messagePointer[BINDING_ENTRY_DEST_EUI]),euiString);
    cJSON_AddStringToObject(tableEntryJson, "destEui", euiString);
    
    cJSON_AddIntegerToObject(tableEntryJson, 
                             "destEndpoint", 
                             messagePointer[BINDING_ENTRY_DEST_ENDPOINT]);
    
    cJSON_AddItemToArray(entryArrayJson, tableEntryJson);
    
    messagePointer += BINDING_TABLE_RESPONSE_ENTRY_SIZE;
  }
  
  cJSON_AddItemToObject(objectJson, "bindTable", entryArrayJson);
  
  objectJsonString = 
    cJSON_PrintUnformatted(objectJson);
  emberAfPluginTransportMqttPublish(topic, objectJsonString);
  free(objectJsonString);
  cJSON_Delete(objectJson);
  free(topic);
}

/** @brief RulesPreCommandReceived
 *
 * Called when rules engine sees a pre command received callback
 *
 * @param commandId   Ver.: always
 * @param clusterSpecific   Ver.: always
 * @param clusterId   Ver.: always
 * @param mfgSpecific   Ver.: always
 * @param mfgCode   Ver.: always
 * @param buffer   Ver.: always
 * @param bufLen   Ver.: always
 * @param payloadStartIndex   Ver.: always
 */
void emberAfPluginRulesEngineRulesPreCommandReceivedCallback(
  int8u commandId,
  boolean clusterSpecific,
  int16u clusterId,
  boolean mfgSpecific,
  int16u mfgCode,
  uint8_t* buffer,
  int8u bufLen,
  int8u payloadStartIndex)
{
  char* topic = allocateAndFormMqttGatewayTopic(ZCL_RESPONSE_TOPIC);
  cJSON* cmdResponseJson;
  char* cmdResponseString;
  char* tempString;
  char* dataString = (char*)malloc(2*(bufLen));
  uint8_t i, *bufPtr;
  EmberEUI64 nodeEui64;
  EmberNodeId nodeId = emberAfCurrentCommand()->source;
  getIeeeFromNodeId(nodeId, nodeEui64);
  
  for (i=0; i < (2*bufLen); i++) {
    dataString[i] = 0;
  }
  
  cmdResponseJson = cJSON_CreateObject();
  
  tempString = createTwoByteHexString(clusterId);
  cJSON_AddStringToObject(cmdResponseJson, "clusterId", tempString);
  free(tempString);
  
  tempString = createOneByteHexString(commandId);
  cJSON_AddStringToObject(cmdResponseJson,"commandId",tempString);
  free(tempString);
  
  bufPtr = buffer + payloadStartIndex;
  for (i = 0; i < (bufLen - payloadStartIndex); i++) {
    sprintf(& (dataString[2*i]), "%02X", bufPtr[i]);
  }
  cJSON_AddStringToObject(cmdResponseJson,"commandData",dataString);
  free(dataString);
  
  if (clusterSpecific) {
    cJSON_AddStringToObject(cmdResponseJson,"clusterSpecific","true");
  } else {
    cJSON_AddStringToObject(cmdResponseJson,"clusterSpecific","false");
  }
  
  if (mfgSpecific) {
    tempString = createTwoByteHexString(mfgCode);
    cJSON_AddStringToObject(cmdResponseJson,"mfgCode",tempString);
    free(tempString);
  }
  
  // node ID, and EUI 64 go here.
  tempString = createTwoByteHexString(nodeId);
  cJSON_AddStringToObject(cmdResponseJson, "nodeId", tempString);
  free(tempString);
  
  tempString = malloc(EUI64_STRING_LENGTH);
  eui64ToString(nodeEui64, tempString);
  cJSON_AddStringToObject(cmdResponseJson, "nodeEui", tempString);
  free(tempString);

  cmdResponseString = cJSON_PrintUnformatted(cmdResponseJson);
  emberAfPluginTransportMqttPublish(topic, cmdResponseString);
  free(cmdResponseString);
  cJSON_Delete(cmdResponseJson);
  free(topic); 
}

