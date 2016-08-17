// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#ifndef __DEVICE_TABLE_H
#define __DEVICE_TABLE_H

typedef uint8_t NewDeviceState;

enum {
  ND_JUST_JOINED  = 0x00,
  ND_HAVE_ACTIVE  = 0x01,
  ND_HAVE_EP_DESC = 0x02,
  ND_JOINED       = 0x10, 
  ND_UNRESPONSIVE = 0x11,
  ND_LEAVE_SENT   = 0x20,
  ND_LEFT         = 0x30,
  ND_UNKNOWN      = 0xff
};

// address table structure
typedef struct 
{
  EmberNodeId nodeId;
  EmberEUI64 eui64;
  uint8_t endpointCount;
  uint8_t endpoints[5];
  NewDeviceState state; 
  uint8_t retries;
} AddressTableEntry;

// endpoint list structure
typedef struct 
{
  uint8_t endpoint;
  uint8_t numInClusters;
  uint16_t inClustersList[20];
  uint8_t numOutClusters;
  uint16_t outClustersList[20];
  EmberNodeId nodeId;
  uint16_t deviceId;
  uint8_t deviceJoinState;
} EndpointDescriptor;

#define ADDRESS_TABLE_SIZE 250
#define MAX_ENDPOINTS (ADDRESS_TABLE_SIZE * 2)

extern AddressTableEntry addressTable[ADDRESS_TABLE_SIZE];
extern EndpointDescriptor endpointList[MAX_ENDPOINTS];

// defined callbacks
void emberNewEndpointCallback( EndpointDescriptor *p_entry );
void deviceTableSend(uint16_t index);
void deviceTableSendWithEndpoint(uint16_t index, uint8_t endpoint);
void getIeeeFromNodeId(EmberNodeId emberNodeId, EmberEUI64 eui64); 

uint16_t getAddressIndexFromNodeId(EmberNodeId emberNodeId);
uint16_t getEndpointFromNodeIdAndEndpoint( EmberNodeId emberNodeId, uint8_t endpoint);
uint16_t getAddressIndexFromIeee(EmberEUI64 ieeeAddress);

void emberAfPluginDeviceTableMessageSentStatus( EmberNodeId nodeId, EmberStatus status );
void emberAfPluginDeviceTableSendLeave(uint16_t index);
void emberAfPluginDeviceTableMessageReceived( EmberNodeId nodeId );

// callbacks
void emberAfPluginDeviceTableStateChangeCallback( EmberNodeId nodeId, uint8_t state);


#define DEVICE_ID_ON_OFF_SWITCH 0x0000
#define DEVICE_ID_LEVEL_CONTROL_SWITCH 0x0001
#define DEVICE_ID_ON_OFF_OUTPUT 0x0002
#define DEVICE_ID_LEVEL_CONTROL_OUTPUT 0x0003
#define DEVICE_ID_SCENE_SELECTOR 0x0004
#define DEVICE_ID_CONFIG_TOOL 0x0005
#define DEVICE_ID_REMOTE_CONTROL 0x0006
#define DEVICE_ID_COMBINED_INTERFACE 0x0007
#define DEVICE_ID_RANGE_EXTENDER 0x0008
#define DEVICE_ID_MAINS_POWER_OUTLET 0x0009
#define DEVICE_ID_DOOR_LOCK 0x000a
#define DEVICE_ID_DOOR_LOCK_CONTROLLER 0x000b
#define DEVICE_ID_SIMPLE_SENSOR 0x000c
#define DEVICE_ID_CONSUMPTION_AWARENESS_DEVICE 0x000d
#define DEVICE_ID_HOME_GATEWAY 0x0050
#define DEVICE_ID_SMART_PLUG 0x0051
#define DEVICE_ID_WHITE_GOODS 0x0052
#define DEVICE_ID_METER_INTERFACE 0x0053

#define DEVICE_ID_ON_OFF_LIGHT 0x0100
#define DEVICE_ID_DIMMABLE_LIGHT 0x0101
#define DEVICE_ID_COLOR_DIMMABLE_LIGHT 0x0102
#define DEVICE_ID_ON_OFF_LIGHT_SWITCH 0x0103
#define DEVICE_ID_DIMMER_SWITCH 0x0104
#define DEVICE_ID_COLOR_DIMMER_SWITCH 0x0105
#define DEVICE_ID_LIGHT_SENSOR 0x0106
#define DEVICE_ID_OCCUPANCY_SENSOR 0x0107

#define DEVICE_ID_SHADE 0x0200
#define DEVICE_ID_SHADE_CONTROLLER 0x0201
#define DEVICE_ID_WINDOW_COVERING_DEVICE 0x0202
#define DEVICE_ID_WINDOW_COVERING_CONTROLLER 0x0203

#define DEVICE_ID_HEATING_COOLING_UNIT 0x0300
#define DEVICE_ID_THERMOSTAT 0x0301
#define DEVICE_ID_TEMPERATURE_SENSOR 0x0302
#define DEVICE_ID_PUMP 0x0303
#define DEVICE_ID_PUMP_CONTROLLER 0x0304
#define DEVICE_ID_PRESSURE_SENSOR 0x0305
#define DEVICE_ID_FLOW_SENSOR 0x0306
#define DEVICE_ID_MINI_SPLIT_AC 0x0307

#define DEVICE_ID_IAS_CIE 0x0400
#define DEVICE_ID_IAS_ANCILLARY_CONTROL 0x0401
#define DEVICE_ID_IAS_ZONE 0x0402
#define DEVICE_ID_IAS_WARNING 0x0403

#define NULL_NODE_ID 0xffff
#define NULL_INDEX   0xffff
#define ADDRESS_TABLE_SIZE 250
#define MAX_ENDPOINTS (ADDRESS_TABLE_SIZE * 2)

#endif //__DEVICE_TABLE_H
