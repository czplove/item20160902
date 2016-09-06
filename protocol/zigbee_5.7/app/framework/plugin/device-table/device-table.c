// Copyright 2016 Silicon Laboratories, Inc.                                *80*
#include PLATFORM_HEADER

#ifdef EZSP_HOST
  // Includes needed for ember related functions for the EZSP host
  #include "stack/include/error.h"
  #include "stack/include/ember-types.h"
  #include "app/util/ezsp/ezsp-protocol.h"
  #include "app/util/ezsp/ezsp.h"
  #include "app/util/ezsp/serial-interface.h"
  #include "app/util/zigbee-framework/zigbee-device-common.h"
#else
  #include "stack/include/ember.h"
#endif

#include "hal/hal.h"
#include "app/util/serial/command-interpreter2.h"
#include "app/framework/include/af.h"
#include "stack/include/event.h"
#include "app/util/source-route-host.h"
#include <stdlib.h> 

#ifndef EMBEDDED_GATEWAY
  #include <stdio.h>
#endif

#define emberAfAppPrintln(...) emberAfPrintln(EMBER_AF_PRINT_APP,  __VA_ARGS__)

#define APS_OPTION_DISCOVER EMBER_APS_OPTION_RETRY
//#define APS_OPTION_DISCOVER EMBER_APS_OPTION_NONE

#include "app/framework/plugin/device-table/device-table.h"

#define newDeviceEventControl emberAfPluginDeviceTableNewDeviceEventEventControl
#define COMMAND_DELAY_QS  16
#define PJOIN_BROADCAST_PERIOD 1
// With device announce there is only the ZDO sequence number, there is no status code.
#define DEVICE_ANNOUNCE_NODE_ID_OFFSET 1
#define DEVICE_ANNOUNCE_EUI64_OFFSET   (DEVICE_ANNOUNCE_NODE_ID_OFFSET + 2)
#define DEVICE_ANNOUNCE_CAPABILITIES_OFFSET (DEVICE_ANNOUNCE_EUI64_OFFSET + EUI64_SIZE)

static PGM_P PGM statusStrings[] = 
{
  "STANDARD_SECURITY_SECURED_REJOIN",
  "STANDARD_SECURITY_UNSECURED_JOIN",
  "DEVICE_LEFT",
  "STANDARD_SECURITY_UNSECURED_REJOIN",
  "HIGH_SECURITY_SECURED_REJOIN",
  "HIGH_SECURITY_UNSECURED_JOIN",
  "Reserved 6",
  "HIGH_SECURITY_UNSECURED_REJOIN"
  "Reserved 8",
  "Reserved 9",
  "Reserved 10",
  "Reserved 11",
  "Reserved 12",
  "Reserved 13",
  "Reserved 14",
  "Reserved 15"
};

static uint8_t permitJoinBroadcastCounter = (PJOIN_BROADCAST_PERIOD-1);
static EmberStatus broadcastPermitJoin(uint8_t duration);
static EmberStatus unicastPermitJoin(uint8_t duration, EmberNodeId nodeId);
void printSourceRouteTable(void);
uint16_t getEndpointFromNodeIdAndEndpoint( EmberNodeId emberNodeId, uint8_t endpoint);
void deleteEndpoint( uint16_t index );

void updateNodeIdInEndpoints( EmberNodeId oldId, EmberNodeId newId );

EmberStatus getEui64FromNodeId( EmberNodeId emberNodeId, EmberEUI64 eui64);
void printEUI64(uint8_t port, uint8_t *eui64);
void printDeviceId(uint16_t deviceId);

void deviceTableSaveCommand(void);
void deviceTableLoadCommand(void);

void emberAfPluginDeviceTableNewDeviceCallback(EmberEUI64 newNodeEui64);
void emberAfPluginDeviceTableDeviceLeftCallback(EmberEUI64 newNodeEui64); 
void emberAfPluginDeviceTableRejoinDeviceCallback(EmberEUI64 newNodeEui64); 
extern void emAfApsFrameEndpointSetup(uint8_t srcEndpoint,
                                      uint8_t dstEndpoint);

static bool shouldDeviceLeave( EmberNodeId newNodeId );

// cli commands
extern uint8_t *emAfZclBuffer;
extern uint16_t emAfZclBufferLen;
extern uint8_t appZclBuffer[];
extern uint16_t appZclBufferLen;
extern bool zclCmdIsBuilt;
extern EmberApsFrame emberAfResponseApsFrame;

AddressTableEntry addressTable[ADDRESS_TABLE_SIZE];
EndpointDescriptor endpointList[MAX_ENDPOINTS];

uint8_t newDeviceEventState = 0x00;
uint16_t currentDevice;
uint16_t currentEndpoint;

EmberNodeId currentRetryNodeId = 0x0000;
uint8_t currentRetryCount = 0x00;

extern EmberApsFrame globalApsFrame;

uint8_t emberAfGetChildTableSize(void);
EmberStatus emberAfGetChildData(uint8_t index,
                                EmberNodeId *childId,
                                EmberEUI64 childEui64,
                                EmberNodeType *childType);

uint16_t getAddressIndexFromIeee(EmberEUI64 ieeeAddress);

bool isChild(EmberNodeId nodeId)
{
  uint8_t size = emberAfGetChildTableSize();
  uint8_t i;

  for (i = 0; i < size; i++) {
    EmberNodeId childId;
    EmberEUI64 childEui64;
    EmberNodeType childType;
    EmberStatus status = emberAfGetChildData(i,
                                             &childId,
                                             childEui64,
                                             &childType);
    if (childId == nodeId)
      return true;
  }
  return false;
}

void updateNodeIdInEndpoints( EmberNodeId oldId, EmberNodeId newId )
{
  uint16_t i;
  
  for (i = 0; i < MAX_ENDPOINTS; i++) {
    if (endpointList[i].nodeId == oldId) {
      endpointList[i].nodeId = newId;
    }
  }

}

void deleteEui64( EmberEUI64 eui64 )
{
  uint8_t i;

  for (i = 0; i < 8 ; i++)
    eui64[i] = 0xff;
}

void deleteAddressTableEntry( uint16_t index )
{
  // first, delete any endpoints we have
  uint16_t i, epIndex;

  for (i  =0; i < addressTable[index].endpointCount; i++) {
    epIndex = 
      getEndpointFromNodeIdAndEndpoint(addressTable[index].nodeId,
                                       addressTable[index].endpoints[i]);
    if (epIndex != NULL_INDEX) {
      deleteEndpoint(epIndex);
    }
  }

  addressTable[index].nodeId = NULL_NODE_ID;
  deleteEui64(addressTable[index].eui64);
  addressTable[index].state = ND_JUST_JOINED;
}

void initAddressTable( void )
{
  uint16_t index;

  for (index = 0; index < ADDRESS_TABLE_SIZE; index++) {
    deleteAddressTableEntry(index);
  }
}

uint16_t getAddressIndexFromIeee(EmberEUI64 ieeeAddress)
{
  uint16_t index;

  for (index = 0; index < ADDRESS_TABLE_SIZE; ++index) {
    if (MEMCOMPARE((addressTable[index].eui64),(ieeeAddress),EUI64_SIZE)==0) {
      return index;
    }
  }
  return NULL_INDEX; 
}

void getIeeeFromNodeId(EmberNodeId emberNodeId, EmberEUI64 eui64)
{
  uint16_t index;

  for (index = 0; index < ADDRESS_TABLE_SIZE; ++index) {
    if (addressTable[index].nodeId == emberNodeId) {
      MEMCOPY(eui64, addressTable[index].eui64, EUI64_SIZE);
      return;
    }
  }
}

void getIeeeFromIndex(uint16_t index, EmberEUI64 eui64)
{
      MEMCOPY(eui64, addressTable[index].eui64, EUI64_SIZE);
      return;
}

uint16_t getAddressIndexFromNodeId(EmberNodeId emberNodeId)
{
  uint16_t index;

  for (index = 0; index < ADDRESS_TABLE_SIZE; ++index)
    if (addressTable[index].nodeId == emberNodeId) {
      return index;
  }

  return NULL_INDEX; 
}

uint16_t findFreeAddressTableIndex( void )
{
  uint16_t index;

  for (index = 0; index<ADDRESS_TABLE_SIZE; index++)
    if (addressTable[index].nodeId == NULL_NODE_ID)
      return index;
  return NULL_INDEX; 
}

void printEUI64(uint8_t port, uint8_t *eui64) {
  uint8_t i;
  for (i = 8; i > 0; i--) {
    emberSerialPrintf(port, "%X", eui64[i-1]);
  }
}

static void printState( NewDeviceState state)
{
  switch (state) {
  case ND_JUST_JOINED:
  case ND_HAVE_ACTIVE:
  case ND_HAVE_EP_DESC:
    emberSerialPrintf(APP_SERIAL," JOINING");
    break;
  case ND_JOINED:
    emberSerialPrintf(APP_SERIAL," JOINED");
    break;
  case ND_UNRESPONSIVE:
    emberSerialPrintf(APP_SERIAL," UNRESPONSIVE");
    break;
  case ND_LEAVE_SENT:
    emberSerialPrintf(APP_SERIAL," LEAVE_SENT");
    break;
  case ND_LEFT: 
    emberSerialPrintf(APP_SERIAL," LEFT");
    break;
  default:
    break;
  }

}

void printAddressTable( void )
{
  uint16_t index, i, totalDevices = 0;
  uint16_t endpointIndex; 

  for (index = 0; index < ADDRESS_TABLE_SIZE; index++) {
    if (addressTable[index].nodeId != NULL_NODE_ID){
      emberSerialPrintf(APP_SERIAL, "%d %2x:  ", totalDevices, addressTable[index].nodeId);
      printEUI64(APP_SERIAL, addressTable[index].eui64);
      //emberSerialPrintf(APP_SERIAL, " %x", addressTable[index].endpointCount);
      for (i = 0; i < addressTable[index].endpointCount; i++) {
        endpointIndex = getEndpointFromNodeIdAndEndpoint(addressTable[index].nodeId,
                                                         addressTable[index].endpoints[i]);
        if (endpointIndex != NULL_INDEX) {
          emberSerialPrintf(APP_SERIAL, " %x %2x ", addressTable[index].endpoints[i]
                                                  , endpointList[endpointIndex].deviceId);
          printDeviceId(endpointList[endpointIndex].deviceId);
        }
      }
      printState(addressTable[index].state);
      emberSerialPrintf(APP_SERIAL, "\r\n");
      totalDevices++;
    }
  }
  emberSerialPrintf(APP_SERIAL, "Total Devices %d\r\n", totalDevices);
}

uint16_t getEndpointFromNodeId(EmberNodeId emberNodeId, uint16_t startIndex)
{
  uint16_t index;

  for (index = startIndex; index < MAX_ENDPOINTS; index++)
    if (endpointList[index].nodeId == emberNodeId)
      return index;

  return NULL_INDEX;
}

uint16_t getEndpointFromNodeIdAndEndpoint(EmberNodeId emberNodeId, uint8_t endpoint)
{
  uint16_t index;

  for (index = 0; index < MAX_ENDPOINTS; index++)
    if (endpointList[index].nodeId == emberNodeId &&
       endpointList[index].endpoint == endpoint)
      return index;

  return NULL_INDEX;
}

uint16_t findFreeEndpointEntry(void)
{
  uint16_t index;

  for (index = 0; index <= MAX_ENDPOINTS; index++)
    if (endpointList[index].nodeId == 0xffff)
      return index;

  return NULL_INDEX;
}

void deleteEndpoint(uint16_t index)
{
  endpointList[index].nodeId = NULL_NODE_ID;
  endpointList[index].endpoint = 0;
}

void initEndpointList(void)
{
  uint16_t index;

  for (index = 0; index < MAX_ENDPOINTS; ++index)
    deleteEndpoint(index);
}

void printEndpoint(uint16_t nodeId, uint8_t endpoint)
{
  uint16_t index = getEndpointFromNodeIdAndEndpoint(nodeId, endpoint);
  uint16_t i;

  if (index == NULL_INDEX)
    return;

  EndpointDescriptor *pDescriptor = &(endpointList[index]);

  emberSerialPrintf(APP_SERIAL, "%2x %2x %x\r\n", 
                    pDescriptor->nodeId, 
                    pDescriptor->deviceId,
                    pDescriptor->endpoint);
  emberSerialPrintf(APP_SERIAL, "%x", pDescriptor->numInClusters);
  for (i = 0; i < pDescriptor->numInClusters; i++) {
    emberSerialPrintf(APP_SERIAL, " %2x", pDescriptor->inClustersList[i]);
  }
  emberSerialPrintf(APP_SERIAL, "\r\n%x", pDescriptor->numOutClusters);
  for (i = 0; i < pDescriptor->numOutClusters; i++) {
    emberSerialPrintf(APP_SERIAL, " %2x", pDescriptor->outClustersList[i]);
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
}

// ************* devices discovery section of code ******************
void resetRetry(EmberNodeId nodeId);
EmberEventControl newDeviceEventControl;

enum
{
  DEVICE_DISCOVERY_STATE_IDLE = 0x00,
  DEVICE_DISCOVERY_STATE_NODE_RETRY = 0x01,
  DEVICE_DISCOVERY_STATE_NODE_WAITING = 0x02,
  DEVICE_DISCOVERY_STATE_NODE_RECEIVED = 0x03,
  DEVICE_DISCOVERY_STATE_ENDPOINTS_RETRY = 0x04,
  DEVICE_DISCOVERY_STATE_ENDPOINTS_WAITING = 0x05,
  DEVICE_DISCOVERY_STATE_ENDPOINTS_RECEIVED = 0x06,
  DEVICE_DISCOVERY_STATE_SIMPLE_RETRY = 0x07,
  DEVICE_DISCOVERY_STATE_SIMPLE_WAITING = 0x08,
  DEVICE_DISCOVERY_STATE_SIMPLE_RECEIVED = 0x09,
  DEVICE_DISCOVERY_STATE_ATTRIBUTES_RETRY = 0x0a,
  DEVICE_DISCOVERY_STATE_ATTRIBUTES_WAITING = 0x0b,
  DEVICE_DISCOVERY_STATE_ATTRIBUTES_RECEIVED = 0x0c,
  DEVICE_DISCOVERY_STATE_REPORT_CONFIG_RETRY = 0x0d,
  DEVICE_DISCOVERY_STATE_REPORT_CONFIG_WAITING = 0x0e,
  DEVICE_DISCOVERY_STATE_REPORT_CONFIG_ACK_RECEIVED = 0x0f
};

// function to determine if the queried node ID is the current nodeId
bool getCurrentNodeId(EmberNodeId nodeId)
{
  if (currentDevice >= ADDRESS_TABLE_SIZE)
    return false;

  return(nodeId == addressTable[currentDevice].nodeId);
}

//void kickOffDeviceDiscover(uint16_t deviceId, EmberEUI64 ieeeAddress)
void kickOffDeviceDiscover(uint16_t index)
{
  // need to fix this and put the device state into the queue.
  assert(newDeviceEventState == DEVICE_DISCOVERY_STATE_IDLE);

  emberEventControlSetActive(newDeviceEventControl);

  currentDevice = index;
  addressTable[index].retries = 0;
  currentEndpoint = 0;
}

void haveNewDevice(uint16_t index)
{
  if (newDeviceEventState == DEVICE_DISCOVERY_STATE_IDLE) {
    emberSerialPrintf(APP_SERIAL, "successful kickoff %x\r\n", index);
    kickOffDeviceDiscover(index);
  } else {
    emberSerialPrintf(APP_SERIAL, "state not idle %x\r\n", index);
  }
}

uint16_t findUndiscoveredDevice(void)
{
  uint16_t i;

  for (i=0; i<ADDRESS_TABLE_SIZE; i++) {
    if (addressTable[i].state <ND_JOINED && 
       addressTable[i].nodeId != NULL_NODE_ID) {
      emberSerialPrintf(APP_SERIAL, "UNDISCOVERED KICKOFF %2x %x\r\n", 
                        addressTable[i].nodeId, addressTable[i].state);
      return i;
    }
  }

  return NULL_INDEX;
}

void checkDeviceQueue(void)
{
  uint16_t index = findUndiscoveredDevice();

  if (index == NULL_INDEX) {
    // all devices are discovered
    emberSerialPrintf(APP_SERIAL, "QUEUE:  All Joined Devices Discovered\r\n");
    currentDevice = NULL_INDEX;
    return;
  } else {
    resetRetry(addressTable[index].nodeId);
    kickOffDeviceDiscover(index);
  }
}
 

void resetRetry(EmberNodeId nodeId)
{
  currentRetryCount = 0;
  currentRetryNodeId = nodeId;
}

void advanceRetry(EmberNodeId nodeId)
{
  if (nodeId != currentRetryNodeId) {
    resetRetry(nodeId);
    return;
  }
  currentRetryCount++;

  if (currentRetryCount > 7) {
    EmberEUI64 eui64;

    // delete device and kickoff new device discovery
    resetRetry(0x0000);

    if (isChild(nodeId) == false) {
      // send leave command
      getIeeeFromNodeId(nodeId, eui64);
      // emberSendRemoveDevice(nodeId, eui64, eui64);

      emberSerialPrintf(APP_SERIAL, "DELETE %2x\r\n",nodeId);
      deleteAddressTableEntry(getAddressIndexFromNodeId(nodeId));
      emberEventControlSetInactive(newDeviceEventControl);
    }
    newDeviceEventState = DEVICE_DISCOVERY_STATE_IDLE;
    checkDeviceQueue();
  }
}

extern void initiateRouteRepair(EmberNodeId nodeId);

void emberAfPluginDeviceTableNewDeviceEventEventHandler(void)
{
  EmberStatus status;
  uint16_t relayList[ZA_MAX_HOPS];
  uint8_t relayCount;
  uint16_t i;

  emberEventControlSetInactive(newDeviceEventControl);

  if (currentDevice >= ADDRESS_TABLE_SIZE) {
    return;
  }

  if (addressTable[currentDevice].nodeId == NULL_NODE_ID) {
    emberEventControlSetInactive(newDeviceEventControl);
    newDeviceEventState = DEVICE_DISCOVERY_STATE_IDLE;
    checkDeviceQueue();
    if (currentDevice == NULL_INDEX) {
      return;
    }
  }

  emberSerialPrintf(APP_SERIAL, "NEW DEVICE STATE:  %d %x %2x %x\r\n", 
    currentDevice, 
    newDeviceEventState, 
    addressTable[currentDevice].nodeId,
    addressTable[currentDevice].state);

  advanceRetry(addressTable[currentDevice].nodeId);
  if (currentDevice == NULL_INDEX) {
    return;
  }

  // Note:  this does not handle the case where we have multiple endpoints
  // and we fail to discover any endpoint after the first endpoint.  
  // to do that we need to fall through to the endpoint state machine.
  if (addressTable[currentDevice].state == ND_JOINED) {
    emberEventControlSetInactive(newDeviceEventControl);
    newDeviceEventState = DEVICE_DISCOVERY_STATE_IDLE;
    checkDeviceQueue();
  }

  switch (newDeviceEventState) {
    case DEVICE_DISCOVERY_STATE_IDLE:
		// Request the node descriptor for the new device
	  emberNodeDescriptorRequest(addressTable[currentDevice].nodeId,EMBER_AF_DEFAULT_APS_OPTIONS);
      emberEventControlSetDelayQS(newDeviceEventControl, 4);
      newDeviceEventState = DEVICE_DISCOVERY_STATE_ENDPOINTS_RETRY;
      break;
    case DEVICE_DISCOVERY_STATE_ENDPOINTS_RETRY:
      // send out active endpoints request.
      if (currentDevice >= ADDRESS_TABLE_SIZE)
        emberSerialPrintf(APP_SERIAL, "ERROR 1\r\n");

      emberFindSourceRoute(addressTable[currentDevice].nodeId, &relayCount, relayList);

      emberSerialPrintf(APP_SERIAL, "Relay Count %d\r\n", relayCount);
      if (relayCount < 10) {

        ezspSetSourceRoute(addressTable[currentDevice].nodeId, relayCount, relayList);
      }

      status = emberActiveEndpointsRequest(addressTable[currentDevice].nodeId,
                                           APS_OPTION_DISCOVER);
      emberEventControlSetDelayQS(newDeviceEventControl, COMMAND_DELAY_QS);
      newDeviceEventState = DEVICE_DISCOVERY_STATE_ENDPOINTS_WAITING;

      break;

    case DEVICE_DISCOVERY_STATE_ENDPOINTS_WAITING:
      if (currentDevice >= ADDRESS_TABLE_SIZE)
        emberSerialPrintf(APP_SERIAL, "ERROR 2\r\n");
      // kick off discovery
      newDeviceEventState = DEVICE_DISCOVERY_STATE_ENDPOINTS_RETRY;
      emberEventControlSetDelayQS(newDeviceEventControl, COMMAND_DELAY_QS);
      initiateRouteRepair(addressTable[currentDevice].nodeId);    
      break;

    case DEVICE_DISCOVERY_STATE_ENDPOINTS_RECEIVED:
    case DEVICE_DISCOVERY_STATE_SIMPLE_RETRY:
      if (currentDevice >= ADDRESS_TABLE_SIZE)
        emberSerialPrintf(APP_SERIAL, "ERROR 3\r\n");
      // check to see if the endpoints request was successful.  If not, then
      // send out the message again.
      if (addressTable[currentDevice].state == ND_HAVE_ACTIVE) {
        // send endpoint descriptor request
        // Note:  this does not really handle multiple endpoints very well.
        // To do that, we would need to keep track of the current endpoint and
        // only send one simple descriptor request at a time.  
        // But as we are testing with devices with single endpoitns for now, 
        // I have chosen to focus on that area.  

        for (i = 0; i < addressTable[currentDevice].endpointCount; i++) {
          emberFindSourceRoute(addressTable[currentDevice].nodeId, &relayCount, relayList);
          if (relayCount < 10)
            ezspSetSourceRoute(addressTable[currentDevice].nodeId, relayCount, relayList);
          emberSimpleDescriptorRequest(addressTable[currentDevice].nodeId,
                                       addressTable[currentDevice].endpoints[i],
                                       EMBER_AF_DEFAULT_APS_OPTIONS);
          newDeviceEventState = DEVICE_DISCOVERY_STATE_SIMPLE_WAITING;
        }
      } else {
        emberFindSourceRoute(addressTable[currentDevice].nodeId, &relayCount, relayList);
        if (relayCount < 10)
          ezspSetSourceRoute(addressTable[currentDevice].nodeId, relayCount, relayList);
        status = emberActiveEndpointsRequest(addressTable[currentDevice].nodeId,
                                             APS_OPTION_DISCOVER);
      }
      emberEventControlSetDelayQS(newDeviceEventControl, COMMAND_DELAY_QS);

      break;

    case DEVICE_DISCOVERY_STATE_SIMPLE_WAITING:
      if (currentDevice >= ADDRESS_TABLE_SIZE)
        emberSerialPrintf(APP_SERIAL, "ERROR 4\r\n");
      // kick off discovery
      newDeviceEventState = DEVICE_DISCOVERY_STATE_SIMPLE_RETRY;
      emberEventControlSetDelayQS(newDeviceEventControl, COMMAND_DELAY_QS);
      initiateRouteRepair(addressTable[currentDevice].nodeId);    
      break;


    case DEVICE_DISCOVERY_STATE_SIMPLE_RECEIVED:
       if (currentDevice >= ADDRESS_TABLE_SIZE)
        emberSerialPrintf(APP_SERIAL, "ERROR 5\r\n");
     // Check to see if we received a simple descriptor.  TBD:  need to add 
      // support for multiple endpoints.  
      if (addressTable[currentDevice].state >= ND_HAVE_EP_DESC) {
        // we are done
        emberEventControlSetInactive(newDeviceEventControl);
        addressTable[currentDevice].state = ND_JOINED;
        newDeviceEventState = DEVICE_DISCOVERY_STATE_IDLE;

        // New device is set, time to make the callback to indicate a new device
        // has joined.
        emberAfPluginDeviceTableNewDeviceCallback(addressTable[currentDevice].eui64);

        // Save on Node Joined
        deviceTableSaveCommand();
        // unicastPermitJoin(100, addressTable[currentDevice].nodeId);
        checkDeviceQueue();

      } else {
        // re-send endpoint descriptor messages;
        // Note:  this does not really handle multiple endpoints very well.
        // To do that, we would need to keep track of the current endpoint and
        // only send one simple descriptor request at a time.  
        // But as we are testing with devices with single endpoitns for now, 
        // I have chosen to focus on that area.  
        for (i = 0; i < addressTable[currentDevice].endpointCount; i++) {
          emberFindSourceRoute(addressTable[currentDevice].nodeId, &relayCount, relayList);
          if (relayCount < 10)
            ezspSetSourceRoute(addressTable[currentDevice].nodeId, relayCount, relayList);
          emberSimpleDescriptorRequest(addressTable[currentDevice].nodeId,
                                       addressTable[currentDevice].endpoints[i],
                                       EMBER_AF_DEFAULT_APS_OPTIONS);
          newDeviceEventState = DEVICE_DISCOVERY_STATE_SIMPLE_WAITING;
        }
        emberEventControlSetDelayQS(newDeviceEventControl, COMMAND_DELAY_QS);

      }
  default:
    break;
  }

}

void newDeviceParseEndpointDescriptorResponse(EmberNodeId nodeId,
                                              EmberApsFrame* apsFrame,
                                              uint8_t* message,
                                              uint16_t length)
{
  uint16_t endpoint, index, i;
  uint8_t *p_count, *p_cluster;
  EndpointDescriptor *p_entry;

  emberSerialPrintf(APP_SERIAL, "simple descriptor node ID: 0x%2x\r\n", nodeId);

  if (getCurrentNodeId(nodeId)) {
    newDeviceEventState = DEVICE_DISCOVERY_STATE_SIMPLE_RECEIVED;
  }

  emberSerialPrintf(APP_SERIAL, "Parsing Endpoint Descriptor %2x %x %x\r\n", nodeId, message[1], message[5]);

  if (message[1] != 0x00)
    // status != success
    return;
  endpoint = message[5];

  index = getEndpointFromNodeIdAndEndpoint(nodeId, endpoint);
  if (index == NULL_INDEX) {
    index = findFreeEndpointEntry();
    if (index == NULL_INDEX) {
      emberSerialPrintf(APP_SERIAL, "Error:  Endpoint Table Full\r\n");
      return;
    }
  } else {
    // we already parsed this endpoint.  Don't re-parse
    return;
  }

  p_entry = &(endpointList[index]);

  p_entry->endpoint = endpoint;
  p_entry->nodeId = nodeId;
  p_entry->deviceId = HIGH_LOW_TO_INT(message[9], message[8]);
  p_count = &(message[11]);
  p_entry->numInClusters = *p_count;
  p_cluster = &(message[12]);
  for (i = 0; i < *p_count; i++) {
    p_entry->inClustersList[i] = HIGH_LOW_TO_INT(p_cluster[1], p_cluster[0]);
    p_cluster = p_cluster + 2;
  }
  p_count = p_cluster;
  p_entry->numOutClusters = *p_count;
  p_cluster ++;
  for (i = 0; i < *p_count; i++) {
    p_entry->outClustersList[i] = HIGH_LOW_TO_INT(p_cluster[1], p_cluster[0]);
    p_cluster = p_cluster + 2;
  }

  emberNewEndpointCallback(p_entry);

  index = getAddressIndexFromNodeId(nodeId);
  currentEndpoint++;
  if(currentEndpoint == addressTable[index].endpointCount)
      if (addressTable[index].state < ND_HAVE_EP_DESC) {

        addressTable[index].state = ND_HAVE_EP_DESC;
        emberEventControlSetActive(newDeviceEventControl);
      }
}

void newDeviceParseActiveEndpointsResponse(EmberNodeId emberNodeId,
                                           EmberApsFrame* apsFrame,
                                           uint8_t* message,
                                           uint16_t length)
{
  uint16_t index = getAddressIndexFromNodeId(emberNodeId);
  uint16_t i;

  if (getCurrentNodeId(emberNodeId)) {
    newDeviceEventState = DEVICE_DISCOVERY_STATE_ENDPOINTS_RECEIVED;
  }

  emberSerialPrintf(APP_SERIAL, "ParseActiveEndpoint\r\n");

  if (index == NULL_NODE_ID) {
    emberSerialPrintf(APP_SERIAL, "Error:  No Valid Address Table Entry\r\n");
    // do IEEE discovery
    return;
  }
  emberSerialPrintf(APP_SERIAL, "Active Endpoint Count: %d\r\n", message[4]);

  // make sure I have not used the redundant endpoint response
  if (addressTable[index].state < ND_HAVE_ACTIVE) {

    addressTable[index].endpointCount = message[4];
    addressTable[index].state = ND_HAVE_ACTIVE;
    emberEventControlSetActive(newDeviceEventControl);

    for (i = 0; i < message[4]; i++) {
      addressTable[index].endpoints[i] = message[5+i];
    }
  }
  return;

}
void newDeviceParseEndDeviceAnnounce(EmberNodeId emberNodeId,
                                     EmberApsFrame* apsFrame,
                                     uint8_t* message,
                                     uint16_t length)
{
    EmberEUI64 eui64;
    emberSerialPrintf(APP_SERIAL, "ParseEndDeviceAnnounce\r\n");

    MEMMOVE(eui64, &(message[DEVICE_ANNOUNCE_EUI64_OFFSET]), EUI64_SIZE);

    uint16_t index = getAddressIndexFromIeee(eui64);

   if (addressTable[index].state < ND_HAVE_ACTIVE) {
       addressTable[index].capabilities = message[DEVICE_ANNOUNCE_CAPABILITIES_OFFSET];
	   addressTable[index].nodeId = emberNodeId;
    }
}
void newDeviceParseNodeDescriptorResponse(EmberNodeId emberNodeId,
                                          EmberApsFrame* apsFrame,
                                          uint8_t* message,
                                          uint16_t length)
{
    uint16_t index = getAddressIndexFromNodeId(emberNodeId);
    emberSerialPrintf(APP_SERIAL, "ParseNodeDescriptorResponse\r\n");

	NodeDescriptor *p_node =  (NodeDescriptor*)&(message[4]);
	if (addressTable[index].state <= ND_JOINED) {
		addressTable[index].logicalType = p_node->logicalType;
	}
}

void newDeviceJoinHandler(EmberNodeId newNodeId,
                          EmberEUI64 newNodeEui64)
{
  printEUI64(APP_SERIAL, newNodeEui64);

  uint16_t index = getAddressIndexFromIeee(newNodeEui64);

  if (index == NULL_INDEX) {
    emberSerialPrintf(APP_SERIAL," New Device Index: %d 0x%2x\r\n",index, newNodeId);

    index = findFreeAddressTableIndex();
    if (index == NULL_INDEX) {
      // error case... no more room in the index table
      emberSerialPrintf(APP_SERIAL, "Error:  Address Table Full\r\n");
      return; 
    }
    addressTable[index].nodeId = newNodeId;
    MEMCOPY(addressTable[index].eui64, newNodeEui64, EUI64_SIZE);

    addressTable[index].state = ND_JUST_JOINED;

    haveNewDevice(index);
  } else {
	emberSerialPrintf(APP_SERIAL, " Device Index:  %d 0x%2x\r\n",index, newNodeId);
    // is this a new node ID?
    if (newNodeId != addressTable[index].nodeId) {
      emberSerialPrintf(APP_SERIAL, 
                        "Node ID Change:  was %2x, is %2x\r\n",
                        addressTable[index].nodeId, newNodeId);

      emberSerialPrintf(APP_SERIAL, ">pre update node\r\n");

      updateNodeIdInEndpoints( addressTable[index].nodeId, newNodeId );

      emberSerialPrintf(APP_SERIAL, ">post update node\r\n");

      addressTable[index].nodeId = newNodeId;

      emberSerialPrintf(APP_SERIAL, ">post update node b\r\n");

      // test code for failure to see leave request.
      { 
        uint16_t endpointIndex = getEndpointFromNodeIdAndEndpoint(
          addressTable[index].nodeId,
          addressTable[index].endpoints[0]);
        EndpointDescriptor *pEndpoint;

        if (endpointIndex == 0xffff) {
          return;
        }

        emberSerialPrintf(APP_SERIAL, ">post update node c\r\n");

        pEndpoint = &(endpointList[endpointIndex]);
        emberSerialPrintf(APP_SERIAL, ">post update node d %2x\r\n", endpointIndex);
        emberNewEndpointCallback( pEndpoint );
        emberSerialPrintf(APP_SERIAL, ">post update node e\r\n");
      }

      // New device is set, time to make the callback to indicate a new device
      // has joined.
      emberAfPluginDeviceTableRejoinDeviceCallback(addressTable[index].eui64);
    }
  }
}

void newDeviceLeftHandler(EmberEUI64 newNodeEui64)
{
  uint16_t index = getAddressIndexFromIeee(newNodeEui64);

  emberAfPluginDeviceTableDeviceLeftCallback(newNodeEui64); 
  if (index != NULL_INDEX)
  {
    deleteAddressTableEntry(index);
    // Save on Node Left
    deviceTableSaveCommand();
  }
}

static EmberStatus broadcastPermitJoin(uint8_t duration)
{
  permitJoinBroadcastCounter++;
  EmberStatus status;

  if (permitJoinBroadcastCounter == PJOIN_BROADCAST_PERIOD) {
    EmberStatus status;
    uint8_t data[3] = { 0,   // sequence number (filled in later)
                        0,   // duration (filled in below)
                        0 }; // TC significance (not used)
    permitJoinBroadcastCounter = 0;

    data[1] = duration;
    status = emberSendZigDevRequest(EMBER_BROADCAST_ADDRESS,
                                    PERMIT_JOINING_REQUEST,
                                    0,   // APS options
                                    data,
                                    3);  // length
  } else {
    status = 0;
  }

  return status;
}

static EmberStatus unicastPermitJoin(uint8_t duration, EmberNodeId nodeId)
{
  EmberStatus status;
  uint8_t data[3] = { 0,   // sequence number (filled in later)
                      0,   // duration (filled in below)
                      0 }; // TC significance (not used)

  data[1] = duration;
  status = emberSendZigDevRequest(nodeId,
                                  PERMIT_JOINING_REQUEST,
                                  0,   // APS options
                                  data,
                                  3);  // length
  return status;
}


void emberAfPluginConcentratorUpdateEventHandler(void);

void gatewayRouteRepair(EmberEUI64 eui64)
{
  emberAfPluginConcentratorUpdateEventHandler();
}

// external function with error checking
EmberStatus getEui64FromNodeId( EmberNodeId emberNodeId, EmberEUI64 eui64) {
  uint16_t i;
  
  for (i = 0; i < ADDRESS_TABLE_SIZE; i++) {
    if (emberNodeId == addressTable[i].nodeId) {
      MEMCOPY(eui64, addressTable[i].eui64, EUI64_SIZE);
      return EMBER_SUCCESS;
    }
  }

  return EMBER_ERR_FATAL;    
}

void printDeviceId(uint16_t deviceId)
{
  switch (deviceId) {
    
  case DEVICE_ID_ON_OFF_SWITCH:
    emberSerialPrintf(APP_SERIAL, "ON_OFF_SWITCH");
    break;
    
  case DEVICE_ID_LEVEL_CONTROL_SWITCH:
    emberSerialPrintf(APP_SERIAL, "LEVEL_CONTROL_SWITCH");
    break;
    
  case DEVICE_ID_ON_OFF_OUTPUT:
    emberSerialPrintf(APP_SERIAL, "ON_OFF_OUTPUT");
    break;
    
  case DEVICE_ID_LEVEL_CONTROL_OUTPUT:
    emberSerialPrintf(APP_SERIAL, "LEVEL_CONTROL_OUTPUT");
    break;
    
  case DEVICE_ID_SCENE_SELECTOR:
    emberSerialPrintf(APP_SERIAL, "SCENE_SELECTOR");
    break;
    
  case DEVICE_ID_CONFIG_TOOL:
    emberSerialPrintf(APP_SERIAL, "CONFIG_TOOL");
    break;
    
  case DEVICE_ID_REMOTE_CONTROL:
    emberSerialPrintf(APP_SERIAL, "REMOTE_CONTROL");
    break;
    
  case DEVICE_ID_COMBINED_INTERFACE:
    emberSerialPrintf(APP_SERIAL, "COMBINED_INTERFACE");
    break;
    
  case DEVICE_ID_RANGE_EXTENDER:
    emberSerialPrintf(APP_SERIAL, "RANGE_EXTENDER");
    break;
    
  case DEVICE_ID_MAINS_POWER_OUTLET:
    emberSerialPrintf(APP_SERIAL, "MAINS_POWER_OUTLET");
    break;
    
  case DEVICE_ID_DOOR_LOCK:
    emberSerialPrintf(APP_SERIAL, "DOOR_LOCK");
    break;
    
  case DEVICE_ID_DOOR_LOCK_CONTROLLER:
    emberSerialPrintf(APP_SERIAL, "DOOR_LOCK_CONTROLLER");
    break;
    
  case DEVICE_ID_SIMPLE_SENSOR:
    emberSerialPrintf(APP_SERIAL, "SIMPLE_SENSOR");
    break;
    
  case DEVICE_ID_CONSUMPTION_AWARENESS_DEVICE:
    emberSerialPrintf(APP_SERIAL, "CONSUMPTION_AWARENESS_DEVICE");
    break;
    
  case DEVICE_ID_HOME_GATEWAY:
    emberSerialPrintf(APP_SERIAL, "HOME_GATEWAY");
    break;
    
  case DEVICE_ID_SMART_PLUG:
    emberSerialPrintf(APP_SERIAL, "SMART_PLUG");
    break;
    
  case DEVICE_ID_WHITE_GOODS:
    emberSerialPrintf(APP_SERIAL, "WHITE_GOODS");
    break;
    
  case DEVICE_ID_METER_INTERFACE:
    emberSerialPrintf(APP_SERIAL, "METER_INTERFACE");
    break;
    
  case DEVICE_ID_ON_OFF_LIGHT:
    emberSerialPrintf(APP_SERIAL, "ON_OFF_LIGHT");
    break;
    
  case DEVICE_ID_DIMMABLE_LIGHT:
    emberSerialPrintf(APP_SERIAL, "DIMMABLE_LIGHT");
    break;
    
  case DEVICE_ID_COLOR_DIMMABLE_LIGHT:
    emberSerialPrintf(APP_SERIAL, "COLOR_DIMMABLE_LIGHT");
    break;
    
  case DEVICE_ID_ON_OFF_LIGHT_SWITCH:
    emberSerialPrintf(APP_SERIAL, "ON_OFF_LIGHT_SWITCH");
    break;
    
  case DEVICE_ID_DIMMER_SWITCH:
    emberSerialPrintf(APP_SERIAL, "DIMMER_SWITCH");
    break;
    
  case DEVICE_ID_COLOR_DIMMER_SWITCH:
    emberSerialPrintf(APP_SERIAL, "COLOR_DIMMER_SWITCH");
    break;
    
  case DEVICE_ID_LIGHT_SENSOR:
    emberSerialPrintf(APP_SERIAL, "LIGHT_SENSOR");
    break;
    
  case DEVICE_ID_OCCUPANCY_SENSOR:
    emberSerialPrintf(APP_SERIAL, "OCCUPANCY_SENSOR");
    break;
    
  case DEVICE_ID_SHADE:
    emberSerialPrintf(APP_SERIAL, "SHADE");
    break;
    
  case DEVICE_ID_SHADE_CONTROLLER:
    emberSerialPrintf(APP_SERIAL, "SHADE_CONTROLLER");
    break;
    
  case DEVICE_ID_WINDOW_COVERING_DEVICE:
    emberSerialPrintf(APP_SERIAL, "WINDOW_COVERING_DEVICE");
    break;
    
  case DEVICE_ID_WINDOW_COVERING_CONTROLLER:
    emberSerialPrintf(APP_SERIAL, "WINDOW_COVERING_CONTROLLER");
    break;
    
  case DEVICE_ID_HEATING_COOLING_UNIT:
    emberSerialPrintf(APP_SERIAL, "HEATING_COOLING_UNIT");
    break;
    
  case DEVICE_ID_THERMOSTAT:
    emberSerialPrintf(APP_SERIAL, "THERMOSTAT");
    break;
    
  case DEVICE_ID_TEMPERATURE_SENSOR:
    emberSerialPrintf(APP_SERIAL, "TEMPERATURE_SENSOR");
    break;
    
  case DEVICE_ID_PUMP:
    emberSerialPrintf(APP_SERIAL, "PUMP");
    break;
    
  case DEVICE_ID_PUMP_CONTROLLER:
    emberSerialPrintf(APP_SERIAL, "PUMP_CONTROLLER");
    break;
    
  case DEVICE_ID_PRESSURE_SENSOR:
    emberSerialPrintf(APP_SERIAL, "PRESSURE_SENSOR");
    break;
    
  case DEVICE_ID_FLOW_SENSOR:
    emberSerialPrintf(APP_SERIAL, "FLOW_SENSOR");
    break;
    
  case DEVICE_ID_MINI_SPLIT_AC:
    emberSerialPrintf(APP_SERIAL, "MINI_SPLIT_AC");
    break;
    
  case DEVICE_ID_IAS_CIE:
    emberSerialPrintf(APP_SERIAL, "IAS_CIE");
    break;
    
  case DEVICE_ID_IAS_ANCILLARY_CONTROL:
    emberSerialPrintf(APP_SERIAL, "IAS_ANCILLARY_CONTROL");
    break;
    
  case DEVICE_ID_IAS_ZONE:
    emberSerialPrintf(APP_SERIAL, "IAS_ZONE");
    break;
    
  case DEVICE_ID_IAS_WARNING:
    emberSerialPrintf(APP_SERIAL, "IAS_WARNING");
    break;
    
  default:
    break;
  }

}

void emberAfPluginDeviceTableInitCallback(void)
{
  initAddressTable();
  initEndpointList();

  // Load on Init
  deviceTableLoadCommand();
}

void clearDeviceTable(void)
{
  initAddressTable();
  initEndpointList();

  deviceTableSaveCommand();
  emberAfPluginDeviceTableClearedCallback();
}

/** @brief Trust Center Join
 *
 * This callback is called from within the application framework's
 * implementation of emberTrustCenterJoinHandler or
 * ezspTrustCenterJoinHandler. This callback provides the same arguments
 * passed to the TrustCenterJoinHandler. For more information about the
 * TrustCenterJoinHandler please see documentation included in
 * stack/include/trust-center.h.
 *
 * @param newNodeId   Ver.: always
 * @param newNodeEui64   Ver.: always
 * @param parentOfNewNode   Ver.: always
 * @param status   Ver.: always
 * @param decision   Ver.: always
 */
void emberAfTrustCenterJoinCallback(EmberNodeId newNodeId,
                                    EmberEUI64 newNodeEui64,
                                    EmberNodeId parentOfNewNode,
                                    EmberDeviceUpdate status,
                                    EmberJoinDecision decision)
{
  uint8_t i;

  emberSerialPrintf(APP_SERIAL,
                    "\r\nTC Join Callback %2x ",
                    newNodeId);

  for (i = 0; i < 8; i++) {
    emberSerialPrintf(APP_SERIAL,
                      "%x",
                      newNodeEui64[7-i]);
  }

  if (status < 16)
    emberSerialPrintf(APP_SERIAL, " %s\r\n", statusStrings[status]);
  else 
    emberSerialPrintf(APP_SERIAL, " %d\r\n", status);

  switch (status) {
  case EMBER_STANDARD_SECURITY_UNSECURED_JOIN:
    // If a new device did an unsecure join, we need to turn on permit joining, as
    // there may be more coming
    // broadcast permit joining to new router as it joins.  
//    broadcastPermitJoin(40);
    newDeviceJoinHandler(newNodeId, newNodeEui64);
    break;
  case EMBER_DEVICE_LEFT:
    newDeviceLeftHandler(newNodeEui64);
    break;
  default:
    // if the device is in the left sent state, we want to send another
    // left message.
    if (shouldDeviceLeave(newNodeId)) {
      return;
    } else {
      newDeviceJoinHandler(newNodeId, newNodeEui64);
    }
    break;
  }
}

/** @brief Pre ZDO Message Received
 *
 * This function passes the application an incoming ZDO message and gives the
 * appictation the opportunity to handle it. By default, this callback returns
 * false indicating that the incoming ZDO message has not been handled and
 * should be handled by the Application Framework.
 *
 * @param emberNodeId   Ver.: always
 * @param apsFrame   Ver.: always
 * @param message   Ver.: always
 * @param length   Ver.: always
 */
void printBuffer(uint8_t *buffer, uint16_t bufLen)
{
  int i;

  for (i = 0; i < bufLen; i++ )
  {
    emberSerialPrintf(APP_SERIAL,
                      "%x ",
                      buffer[i]);
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
}

bool emberAfPreZDOMessageReceivedCallback(EmberNodeId emberNodeId,
                                          EmberApsFrame* apsFrame,
                                          uint8_t* message,
                                          uint16_t length)
{
  emberSerialPrintf(APP_SERIAL,"%2x:  ", emberNodeId);
  switch (apsFrame->clusterId)
  {
  case ACTIVE_ENDPOINTS_RESPONSE:
    emberSerialPrintf(APP_SERIAL,"Active Endpoints Response\r\n");
    newDeviceParseActiveEndpointsResponse( emberNodeId,
                                           apsFrame,
                                           message,
                                           length);
    break;
  case SIMPLE_DESCRIPTOR_RESPONSE:
    emberSerialPrintf(APP_SERIAL,"Simple Descriptor Response\r\n");
    
    newDeviceParseEndpointDescriptorResponse(emberNodeId,
                                             apsFrame,
                                             message,
                                             length);
    break;
  case END_DEVICE_ANNOUNCE:
    emberSerialPrintf(APP_SERIAL,"End Device Announce\r\n");
    newDeviceParseEndDeviceAnnounce(emberNodeId,
                                    apsFrame,
                                    message,
                                    length);
    break;
  case NODE_DESCRIPTOR_RESPONSE:
    emberSerialPrintf(APP_SERIAL,"Node Descriptor Response\r\n");
	newDeviceParseNodeDescriptorResponse(emberNodeId,
                                         apsFrame,
                                         message,
                                         length);
	return false;
	break;
  case PERMIT_JOINING_RESPONSE:
    emberSerialPrintf(APP_SERIAL,"Permit Joining Response: ");
    printBuffer(message, length);
    break;
  case LEAVE_RESPONSE:
    emberSerialPrintf(APP_SERIAL,"Network Leave Response: ");
    printBuffer(message, length);
    break;
  case BIND_RESPONSE:
    emberAfPluginDeviceTableBindResponseCallback(emberNodeId,
                                                 apsFrame,
                                                 message,
                                                 length);
    break;
  case BINDING_TABLE_RESPONSE:
    emberAfPluginDeviceTableBindingTableResponseCallback(emberNodeId,
                                                         apsFrame,
                                                         message,
                                                         length);
    break;
  default:
    emberSerialPrintf(APP_SERIAL,"Untracked ZDO\r\n", apsFrame->clusterId);
    break;
  
  }
/*
  emberSerialPrintf(APP_SERIAL,
                    "%2x ",
                    emberNodeId);

  printBuffer(message, length);
  */

  return false;
}

// command to send the CIE IEEE address to the IAS Zone cluster
void sendCieAddressWrite( EmberNodeId nodeId, uint8_t endpoint )
{
  uint32_t i;
  EmberEUI64 eui64;
  uint8_t outgoingBuffer[15];

  emberAfGetEui64(eui64);

  globalApsFrame.options = EMBER_AF_DEFAULT_APS_OPTIONS;
  globalApsFrame.clusterId = ZCL_IAS_ZONE_CLUSTER_ID;
  globalApsFrame.sourceEndpoint = 0x01;
  globalApsFrame.destinationEndpoint = endpoint;

  outgoingBuffer[0] = 0x00;
  outgoingBuffer[1] = emberAfNextSequence();
  outgoingBuffer[2] = ZCL_WRITE_ATTRIBUTES_COMMAND_ID;
  outgoingBuffer[3] = LOW_BYTE(ZCL_IAS_CIE_ADDRESS_ATTRIBUTE_ID);
  outgoingBuffer[4] = HIGH_BYTE(ZCL_IAS_CIE_ADDRESS_ATTRIBUTE_ID);
  outgoingBuffer[5] = ZCL_IEEE_ADDRESS_ATTRIBUTE_TYPE;

  for (i = 0; i < 8; i++) {
    outgoingBuffer[6+i] = eui64[i];
  }
                  
  emberAfSendUnicast(EMBER_OUTGOING_DIRECT,
                     nodeId,
                     &globalApsFrame,
                     14,
                     outgoingBuffer);
}

// We have a new endpoint, and we have discovered its attributes.  Figure out
// if we need to do anything, like write the CIE address to it.

void emberNewEndpointCallback( EndpointDescriptor *p_entry )
{
  uint8_t i;

  for (i = 0; i < p_entry->numInClusters ;i++)
  {
    emberSerialPrintf(APP_SERIAL, "%2x ", p_entry->inClustersList[i]);

    if (p_entry->inClustersList[i] == ZCL_IAS_ZONE_CLUSTER_ID) {
      // write IEEE address to CIE address location
      //sendCieAddressWrite(p_entry->nodeId, p_entry->endpoint);
		//uint8_t addrIndex = getAddressIndexFromNodeId(p_entry->nodeId);
		//checkForIasZoneServer(p_entry->nodeId,addressTable[addrIndex].eui64);
      break;
    }
  }
  emberSerialPrintf(APP_SERIAL, "\r\n");
}

void deviceTableSendWithEndpoint(uint16_t index, uint8_t endpoint)
{
  EmberNodeId nodeId;
  EmberStatus status;

  nodeId = addressTable[index].nodeId;
  endpoint = endpoint;

  emberAfSetCommandEndpoints(1, endpoint);
  status = emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT, nodeId);
  emberSerialPrintf(APP_SERIAL,"Send Status %x\r\n", status);
}

void deviceTableSend(uint16_t index)
{
  uint8_t endpoint = addressTable[index].endpoints[0];
  deviceTableSendWithEndpoint(index, endpoint);
}

void deviceTableSendCommand(void)
{
  uint16_t index = (uint8_t)emberUnsignedCommandArgument(0);

  deviceTableSend(index);
}

static EmberNodeId discoverNode;

void deviceTabeCliServiceDiscoveryCallback(const EmberAfServiceDiscoveryResult* result)
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

void deviceTableDiscoverPresentNodeCommand(void)
{
  discoverNode = (EmberNodeId)emberUnsignedCommandArgument(0);

  // first, determine if this is a child of ours

  emberAfFindIeeeAddress(discoverNode,
                         deviceTabeCliServiceDiscoveryCallback);
}

// ****************************************************************************
// 
// Save/load of the device table.  
void deviceTableSaveCommand(void)
{
#ifndef EMBEDDED_GATEWAY
  FILE *fp;
  uint16_t i, j;
  
  EndpointDescriptor *epl = endpointList;

  // save address Table
  fp = fopen("devices.txt", "w");
  
  for (i = 0 ; i < ADDRESS_TABLE_SIZE; i++) {
    if (addressTable[i].nodeId != NULL_NODE_ID) {
      fprintf(fp, "%x %x ", addressTable[i].nodeId, addressTable[i].endpointCount);
      fprintf(fp,"%x ",addressTable[i].logicalType);
      fprintf(fp,"%x ",addressTable[i].capabilities);
      for (j = 0; j < 8; j++) {
        fprintf(fp, "%x ", addressTable[i].eui64[j]);
      }
      for (j  =0 ; j < 5; j++) {
        fprintf(fp, "%x ", addressTable[i].endpoints[j]);
      }
    }
  }
  // write something to mark the end
  fprintf(fp, "\r\nffff\r\n");
  
  fclose(fp);
  
  // save endpoint table
  fp = fopen("endpoints.txt", "w");
  
  for (i = 0; i < MAX_ENDPOINTS; i++) {
    if (epl[i].nodeId != NULL_NODE_ID) {
      
      fprintf(fp, "%x %x %x %x ",
              epl[i].nodeId, epl[i].deviceId, epl[i].endpoint,
              epl[i].deviceJoinState);
      fprintf(fp, "%x ", epl[i].numInClusters);
      for (j = 0; j < epl[i].numInClusters; j++) {
        fprintf(fp, "%x ", epl[i].inClustersList[j]);
      }
      fprintf(fp, "%x ", epl[i].numOutClusters);
      for (j = 0; j < epl[i].numOutClusters; j++) {
        fprintf(fp, "%x ", epl[i].outClustersList[j]);
      }
    }
  }
    
  // write something to mark the end
  fprintf(fp, "\r\nffff\r\n");

  fclose(fp);
    
#endif
}

void deviceTableLoadCommand(void)
{
#ifndef EMBEDDED_GATEWAY
  FILE *fp;
  uint16_t i, j; 

  unsigned int data, data2;

  EndpointDescriptor *epl = endpointList;

  fp = fopen("devices.txt", "r");

  if (!fp) {
    return;
  }

  for (i = 0; i < ADDRESS_TABLE_SIZE && feof(fp) == false; i++) {
    fscanf(fp, "%x %x ", &(addressTable[i].nodeId), &data);
    addressTable[i].endpointCount = (uint8_t) data;
    fscanf(fp,"%x ",&data);
    addressTable[i].logicalType = (uint8_t) data;
    fscanf(fp,"%x ",&data);
    addressTable[i].capabilities = (uint8_t) data;
    if (addressTable[i].nodeId != NULL_NODE_ID) {
      for (j = 0; j < 8; j++) {
        fscanf(fp, "%x", &data);
        addressTable[i].eui64[j] = (uint8_t) data;
      }
      for (j  =0; j < 5; j++) {
        fscanf(fp, "%x", &data);
        addressTable[i].endpoints[j] = (uint8_t) data;
      }
      addressTable[i].state = ND_JOINED;
    }
  }

  fclose(fp);

  // set the rest of the address table to null.
  for (; i < ADDRESS_TABLE_SIZE ; i++) {
    addressTable[i].nodeId = NULL_NODE_ID;
  }

  // load endpoint table
  fp = fopen("endpoints.txt", "r");

  for (i = 0; i < MAX_ENDPOINTS && feof(fp) == false; i++) {
    fscanf(fp, "%x %x %x %x",
           &(epl[i].nodeId), &(epl[i].deviceId), &data, &data2);
    epl[i].endpoint = (uint8_t) data;
    epl[i].deviceJoinState = (uint8_t) data2;
    
    if (epl[i].nodeId != NULL_NODE_ID) {
      fscanf(fp, "%x ", &data);
      epl[i].numInClusters = (uint8_t) data;
      for (j=0; j< epl[i].numInClusters; j++) {
        fscanf(fp, "%x ", &data);
        epl[i].inClustersList[j] = (uint8_t) data;
      }
      fscanf(fp, "%x ", &data);
      epl[i].numOutClusters = (uint8_t) data;
      for (j = 0; j < epl[i].numOutClusters; j++) {
        fscanf(fp, "%x ", &data);
        epl[i].outClustersList[j] = (uint8_t) data;
      }
    }
  }
    
  // write something to mark the end
  fprintf(fp, "\r\nffff\r\n");

  fclose(fp);

#endif
}

// ************ device state tracking
static uint8_t optionallyChangeState( EmberNodeId nodeId, NewDeviceState state);

void emberAfPluginDeviceTableSendLeave(uint16_t index)
{
  uint16_t nodeId = addressTable[index].nodeId;
  EmberEUI64 destinationEui64;

  EmberApsOption apsOptions = EMBER_APS_OPTION_RETRY | 
    EMBER_APS_OPTION_ENABLE_ROUTE_DISCOVERY |
    EMBER_APS_OPTION_ENABLE_ADDRESS_DISCOVERY;

  // copy elements from address table
  MEMCOPY(destinationEui64, addressTable[index].eui64, sizeof(EmberEUI64));

  // use the ZDO command to remove the device
  emberLeaveRequest(nodeId,
                    destinationEui64,
                    0x00,        // Just leave.  Do not remove children, if any.
                    apsOptions);
  emberSerialPrintf(APP_SERIAL, "LEAVE_SENT\r\n");

  optionallyChangeState(addressTable[index].nodeId, ND_LEAVE_SENT);
}

static void handleUnknownDevcie(EmberNodeId nodeId)
{
}

static NewDeviceState getCurrentState(EmberNodeId nodeId)
{
  uint16_t index = getAddressIndexFromNodeId(nodeId);

  if (index == 0xffff) {
    return ND_UNKNOWN;
  }

  return addressTable[index].state;
}

// Handle state transitions.  If the requested state is different from the 
// current state, we need to call the state change callback.
static uint8_t optionallyChangeState(EmberNodeId nodeId, NewDeviceState state)
{
  uint16_t index = getAddressIndexFromNodeId(nodeId);
  NewDeviceState originalState;

  if (index == 0xffff) {
    // we don't know this device.  Kick off a discovery process here.
    handleUnknownDevcie(nodeId);
    return ND_JUST_JOINED;
  }

  originalState = addressTable[index].state;

  if (originalState < ND_JOINED) {
    // still in discovery mode...do nothing
    return 0;
  }

  if (addressTable[index].state != state) {
    addressTable[index].state = state;
    emberAfPluginDeviceTableStateChangeCallback(addressTable[index].nodeId, state);
  }

  return originalState;
}

// If we send a message to a device and it was successful, then we label the 
// device as joined.  If we send a message to a devcie and it was unsuccessful, 
// we e sent a message to a device.  Track whether it was successful here.
void emberAfPluginDeviceTableMessageSentStatus(EmberNodeId nodeId, EmberStatus status)
{
  // first, if we are in the leaving state, we do not want to transition
  if (getCurrentState(nodeId) == ND_LEAVE_SENT) {
    return;
  }

  if (status == EMBER_SUCCESS) {
    optionallyChangeState(nodeId, ND_JOINED);
  } else {
    optionallyChangeState(nodeId, ND_UNRESPONSIVE);
  }
}

// From the incoming message handler...if we receive a message from a device,
// we need to make sure its state is JOINED.
void emberAfPluginDeviceTableMessageReceived(EmberNodeId nodeId)
{
  uint16_t index;

  if (getCurrentState(nodeId) == ND_LEAVE_SENT) {
    // We have heard from a device that should have left but hasn't.  Try
    // re-sending the leave message.
    emberAfPluginDeviceTableSendLeave(getAddressIndexFromNodeId(nodeId));
    return;
  }

  optionallyChangeState(nodeId, ND_JOINED);
}

static bool shouldDeviceLeave(EmberNodeId nodeId)
{
  if (getCurrentState(nodeId) == ND_LEAVE_SENT) {
    emberAfPluginDeviceTableSendLeave(getAddressIndexFromNodeId(nodeId));
    return true;
  }

  return false;
}
