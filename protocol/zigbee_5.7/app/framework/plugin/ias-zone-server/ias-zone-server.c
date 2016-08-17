// *****************************************************************************
// * ias-zone-server.c
// *
// * This is the source for the plugin used to add an IAS Zone cluster server
// * to a project.  This source handles zone enrollment and storing of
// * attributes from a CIE device, and provides an API for different plugins to
// * post updated zone status values.
// *
// * Copyright 2015 Silicon Laboratories, Inc.                              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#include "ias-zone-server.h"

#ifdef EMBER_SCRIPTED_TEST
#include "app/framework/plugin/ias-zone-server/ias-zone-server-test.h"
#endif

#define UNDEFINED_ZONE_ID 0xFF
#define DELAY_TIMER_MS 1000

//-----------------------------------------------------------------------------
// Globals

EmberEventControl emberAfPluginIasZoneServerInitEventControl;
static bool areZoneServerAttributesTokenized(uint8_t endpoint);

//-----------------------------------------------------------------------------
// Forward declarations

static void setZoneId(uint8_t endpoint, uint8_t zoneId);

//-----------------------------------------------------------------------------
// Functions

static EmberStatus sendToClient(uint8_t endpoint)
{
  EmberStatus status;

  // If the device is not a network, there is no one to send to, so do nothing
  if (emberAfNetworkState() != EMBER_JOINED_NETWORK) {
    return EMBER_NETWORK_DOWN;
  }

  // Remote endpoint need not be set, since it will be provided by the call to
  // emberAfSendCommandUnicastToBindings()
  emberAfSetCommandEndpoints(endpoint, 0);

  // A binding table entry is created on Zone Enrollment for each endpoint, so
  // a simple call to SendCommandUnicastToBinding will handle determining the
  // destination endpoint, address, etc for us.
  status = emberAfSendCommandUnicastToBindings();

  if (EMBER_SUCCESS != status) {
    return status;
  }

  return status;
}

static void enrollWithClient(uint8_t endpoint)
{
  EmberStatus status;
  emberAfFillCommandIasZoneClusterZoneEnrollRequest(EMBER_AF_PLUGIN_IAS_ZONE_SERVER_ZONE_TYPE,
                                                    EMBER_AF_MANUFACTURER_CODE);
  status = sendToClient(endpoint);
  if (status == EMBER_SUCCESS) {
    emberAfIasZoneClusterPrintln("Sent enroll request to IAS Zone client.");
  } else {
    emberAfIasZoneClusterPrintln("Error sending enroll request: 0x%x\n", status);
  }
}

EmberAfStatus emberAfIasZoneClusterServerPreAttributeChangedCallback(
  uint8_t endpoint,
  EmberAfAttributeId attributeId,
  EmberAfAttributeType attributeType,
  uint8_t size,
  uint8_t *value)
{
  uint8_t i;
  bool zeroAddress;
  EmberBindingTableEntry bindingEntry;
  EmberBindingTableEntry currentBind;
  EmberEUI64 destEUI;
  uint8_t ieeeAddress[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

  // If this is not a CIE Address write, the CIE address has already been
  // written, or the IAS Zone server is already enrolled, do nothing.
  if (attributeId != ZCL_IAS_CIE_ADDRESS_ATTRIBUTE_ID
      || emberAfCurrentCommand() == NULL) {
    return EMBER_ZCL_STATUS_SUCCESS;
  }

  MEMCOPY(destEUI, value, EUI64_SIZE);

  // Create the binding table entry

  // This code assumes that the endpoint and device that is setting the CIE
  // address is the CIE device itself, and as such the remote endpoint to bind
  // to is the endpoint that generated the attribute change.  This
  // assumption is made based on analysis of the behavior of CIE devices
  // currently existing in the field.
  bindingEntry.type = EMBER_UNICAST_BINDING;
  bindingEntry.local = endpoint;
  bindingEntry.clusterId = ZCL_IAS_ZONE_CLUSTER_ID;
  bindingEntry.remote = emberAfCurrentCommand()->apsFrame->sourceEndpoint;
  MEMCOPY(bindingEntry.identifier, destEUI, EUI64_SIZE);

  // Cycle through the binding table until we find a valid entry that is not
  // being used, then use the created entry to make the bind.
  for (i = 0; i < EMBER_BINDING_TABLE_SIZE; i++) {
    if (emberGetBinding(i, &currentBind) != EMBER_SUCCESS) {
      //break out of the loop to ensure that an error message still prints
      break;
    }
    if (currentBind.type != EMBER_UNUSED_BINDING) {
      // If the binding table entry created based on the response already exists
      // do nothing.
      if ((currentBind.local == bindingEntry.local)
           && (currentBind.clusterId == bindingEntry.clusterId)
           && (currentBind.remote == bindingEntry.remote)
           && (currentBind.type == bindingEntry.type)) {
        break;
      }
      // If this spot in the binding table already exists, move on to the next
      continue;
    } else {
      emberSetBinding(i, &bindingEntry);
      break;
    }
  }

  zeroAddress = true;
  emberAfReadServerAttribute(endpoint,
                            ZCL_IAS_ZONE_CLUSTER_ID,
                            ZCL_IAS_CIE_ADDRESS_ATTRIBUTE_ID,
                            (uint8_t*)ieeeAddress,
                            8);
  for (i = 0; i < 8; i++) {
    if (ieeeAddress[i] != 0) {
      zeroAddress = false;
    }
  }
  emberAfAppPrint("\nzero address: %d\n",zeroAddress);

  if (zeroAddress == true) {
    // We need to delay to get around a bug where we can't send a command
    // at this point because then the Write Attributes response will not
    // be sent.  But we also delay to give the client time to configure us.
    emberAfIasZoneClusterPrintln("Sending enrollment after %d ms", DELAY_TIMER_MS);
    emberAfScheduleServerTickExtended(endpoint,
                                      ZCL_IAS_ZONE_CLUSTER_ID,
                                      DELAY_TIMER_MS,
                                      EMBER_AF_SHORT_POLL,
                                      EMBER_AF_STAY_AWAKE);
  }

  return EMBER_ZCL_STATUS_SUCCESS;
}

bool emberAfIasZoneClusterAmIEnrolled(uint8_t endpoint)
{
  EmberAfIasZoneState zoneState;
  EmberAfStatus status;
  status = emberAfReadServerAttribute(endpoint,
                                      ZCL_IAS_ZONE_CLUSTER_ID,
                                      ZCL_ZONE_STATE_ATTRIBUTE_ID,
                                      (unsigned char*)&zoneState,
                                      1);  // uint8_t size
  return (status == EMBER_ZCL_STATUS_SUCCESS
          && zoneState == EMBER_ZCL_IAS_ZONE_STATE_ENROLLED);
}

static void updateEnrollState(uint8_t endpoint, bool enrolled)
{
  uint8_t zoneState = (enrolled
                     ? EMBER_ZCL_IAS_ZONE_STATE_ENROLLED
                     : EMBER_ZCL_IAS_ZONE_STATE_NOT_ENROLLED);
  emberAfWriteServerAttribute(endpoint,
                              ZCL_IAS_ZONE_CLUSTER_ID,
                              ZCL_ZONE_STATE_ATTRIBUTE_ID,
                              (uint8_t*)&zoneState,
                              ZCL_INT8U_ATTRIBUTE_TYPE);
  emberAfIasZoneClusterPrintln("IAS Zone Server State: %pEnrolled",
                               (enrolled
                                ? ""
                                : "NOT "));
}

bool emberAfIasZoneClusterZoneEnrollResponseCallback(uint8_t enrollResponseCode,
                                                        uint8_t zoneId)
{
  uint8_t endpoint;
  uint8_t epZoneId;
  EmberAfStatus status;

  endpoint = emberAfCurrentEndpoint();
  status = emberAfReadServerAttribute(endpoint,
                                    ZCL_IAS_ZONE_CLUSTER_ID,
                                    ZCL_ZONE_ID_ATTRIBUTE_ID,
                                    &epZoneId,
                                    sizeof(uint8_t));
  if (status == EMBER_ZCL_STATUS_SUCCESS) {
    if (enrollResponseCode == EMBER_ZCL_IAS_ENROLL_RESPONSE_CODE_SUCCESS) {
      updateEnrollState(endpoint, true);
      setZoneId(endpoint, zoneId);
    } else {
      updateEnrollState(endpoint, false);
      setZoneId(endpoint, UNDEFINED_ZONE_ID);
    }

    return true;
  }

  emberAfAppPrintln("ERROR: IAS Zone Server unable to read zone ID attribute");
  return true;
}

static EmberStatus sendZoneUpdate(uint8_t timeSinceStatusOccurredQs)
{
  EmberStatus status;
  uint16_t zoneStatus;
  uint8_t endpoint;
  uint8_t i;

  for (i = 0; i < emberAfEndpointCount(); i++) {
    endpoint = emberAfEndpointFromIndex(i);

    if (!emberAfIasZoneClusterAmIEnrolled(endpoint)) {
      return EMBER_INVALID_CALL;
    }
    emberAfReadServerAttribute(endpoint,
                               ZCL_IAS_ZONE_CLUSTER_ID,
                               ZCL_ZONE_STATUS_ATTRIBUTE_ID,
                               (uint8_t*)&zoneStatus,
                               2);  // uint16_t size
    emberAfFillCommandIasZoneClusterZoneStatusChangeNotification(
      zoneStatus,
      0, // extended status, must be zero per the spec.
      emberAfPluginIasZoneServerGetZoneId(endpoint),
      timeSinceStatusOccurredQs);  // known as delay in the spec
    status = sendToClient(endpoint);
  }

  return status;
}

EmberStatus emberAfPluginIasZoneServerUpdateZoneStatus(
  uint8_t endpoint,
  uint16_t newStatus,
  uint8_t timeSinceStatusOccurredQs)
{
  emberAfWriteServerAttribute(endpoint,
                              ZCL_IAS_ZONE_CLUSTER_ID,
                              ZCL_ZONE_STATUS_ATTRIBUTE_ID,
                              (uint8_t*)&newStatus,
                              ZCL_INT16U_ATTRIBUTE_TYPE);
  return sendZoneUpdate(timeSinceStatusOccurredQs);
}

void emberAfIasZoneClusterServerInitCallback(uint8_t endpoint)
{
  EmberAfIasZoneType zoneType;
  if (!areZoneServerAttributesTokenized(endpoint)) {
    emberAfAppPrint("WARNING: ATTRIBUTES ARE NOT BEING STORED IN FLASH! ");
    emberAfAppPrintln("DEVICE WILL NOT FUNCTION PROPERLY AFTER REBOOTING!!");
  }

  zoneType = (EmberAfIasZoneType)EMBER_AF_PLUGIN_IAS_ZONE_SERVER_ZONE_TYPE;
  emberAfWriteAttribute(endpoint,
                        ZCL_IAS_ZONE_CLUSTER_ID,
                        ZCL_ZONE_TYPE_ATTRIBUTE_ID,
                        CLUSTER_MASK_SERVER,
                        (uint8_t*)&zoneType,
                        ZCL_INT16U_ATTRIBUTE_TYPE);

  emberAfPluginIasZoneServerUpdateZoneStatus(endpoint,
                                             0,   // status: All alarms cleared
                                             0);  // time since status occurred

  emberEventControlSetActive(emberAfPluginIasZoneServerInitEventControl);
}

void emberAfPluginIasZoneServerInitEventHandler(void)
{
  EmberAfStatus status;
  EmberAfIasZoneState zoneState;
  uint8_t i;
  bool zoneEnrolled;
  uint8_t endpoint;

  emberEventControlSetInactive(emberAfPluginIasZoneServerInitEventControl);

  // If the device is not on a network, no action is necessary
  if (emberAfNetworkState() != EMBER_JOINED_NETWORK) {
    return;
  }

  // If the device is on a network, verify that at least one zone server is
  // enrolled.  Otherwise, something has gone wrong, the zone server has no
  // client, and the device needs to undergo zone enrollment again by leaving
  // and rejoining the network.
  zoneEnrolled = false;
  for (i = 0; i < emberAfEndpointCount(); i++) {
    endpoint = emberAfEndpointFromIndex(i);
    status = emberAfReadServerAttribute(endpoint,
                                        ZCL_IAS_ZONE_CLUSTER_ID,
                                        ZCL_ZONE_STATE_ATTRIBUTE_ID,
                                        (uint8_t *)&zoneState,
                                        1);  // uint8_t size
    if ((status == EMBER_ZCL_STATUS_SUCCESS)
        && (zoneState == EMBER_ZCL_IAS_ZONE_STATE_ENROLLED)) {
      zoneEnrolled = true;
    }
  }

  if(!zoneEnrolled) {
    emberLeaveNetwork();
  }
}

void emberAfIasZoneClusterServerTickCallback(uint8_t endpoint)
{
  emberAfAppPrintln("Sending enroll from tickcall");
  enrollWithClient(endpoint);
}

uint8_t emberAfPluginIasZoneServerGetZoneId(uint8_t endpoint)
{
  uint8_t zoneId = UNDEFINED_ZONE_ID;
  emberAfReadServerAttribute(endpoint,
                             ZCL_IAS_ZONE_CLUSTER_ID,
                             ZCL_ZONE_ID_ATTRIBUTE_ID,
                             &zoneId,
                             emberAfGetDataSize(ZCL_INT8U_ATTRIBUTE_TYPE));
  return zoneId;
}

//------------------------------------------------------------------------------
//
// This function will verify that all attributes necessary for the IAS zone
// server to properly retain functionality through a power failure are
// tokenized.
//
//------------------------------------------------------------------------------
static bool areZoneServerAttributesTokenized(uint8_t endpoint)
{
  EmberAfAttributeMetadata *metadata;

  metadata = emberAfLocateAttributeMetadata(endpoint,
                                            ZCL_IAS_ZONE_CLUSTER_ID,
                                            ZCL_IAS_CIE_ADDRESS_ATTRIBUTE_ID,
                                            CLUSTER_MASK_SERVER,
                                            EMBER_AF_NULL_MANUFACTURER_CODE);
  if (!emberAfAttributeIsTokenized(metadata)) {
    return false;
  }

  metadata = emberAfLocateAttributeMetadata(endpoint,
                                            ZCL_IAS_ZONE_CLUSTER_ID,
                                            ZCL_ZONE_STATE_ATTRIBUTE_ID,
                                            CLUSTER_MASK_SERVER,
                                            EMBER_AF_NULL_MANUFACTURER_CODE);
  if (!emberAfAttributeIsTokenized(metadata)) {
    return false;
  }

  metadata = emberAfLocateAttributeMetadata(endpoint,
                                            ZCL_IAS_ZONE_CLUSTER_ID,
                                            ZCL_ZONE_TYPE_ATTRIBUTE_ID,
                                            CLUSTER_MASK_SERVER,
                                            EMBER_AF_NULL_MANUFACTURER_CODE);
  if (!emberAfAttributeIsTokenized(metadata)) {
    return false;
  }

  metadata = emberAfLocateAttributeMetadata(endpoint,
                                            ZCL_IAS_ZONE_CLUSTER_ID,
                                            ZCL_ZONE_ID_ATTRIBUTE_ID,
                                            CLUSTER_MASK_SERVER,
                                            EMBER_AF_NULL_MANUFACTURER_CODE);
  if (!emberAfAttributeIsTokenized(metadata)) {
    return false;
  }

  return true;
}

static void setZoneId(uint8_t endpoint, uint8_t zoneId)
{
  emberAfIasZoneClusterPrintln("IAS Zone Server Zone ID: 0x%X", zoneId);
  emberAfWriteServerAttribute(endpoint,
                              ZCL_IAS_ZONE_CLUSTER_ID,
                              ZCL_ZONE_ID_ATTRIBUTE_ID,
                              &zoneId,
                              ZCL_INT8U_ATTRIBUTE_TYPE);
}

static void unenrollSecurityDevice(uint8_t endpoint)
{
 uint8_t ieeeAddress[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
 uint16_t zoneType = EMBER_AF_PLUGIN_IAS_ZONE_SERVER_ZONE_TYPE;

 emberAfWriteServerAttribute(endpoint,
                             ZCL_IAS_ZONE_CLUSTER_ID,
                             ZCL_IAS_CIE_ADDRESS_ATTRIBUTE_ID,
                             (uint8_t*)ieeeAddress,
                             ZCL_IEEE_ADDRESS_ATTRIBUTE_TYPE);

 emberAfWriteServerAttribute(endpoint,
                             ZCL_IAS_ZONE_CLUSTER_ID,
                             ZCL_ZONE_TYPE_ATTRIBUTE_ID,
                             (uint8_t*)&zoneType,
                             ZCL_INT16U_ATTRIBUTE_TYPE);

 setZoneId(endpoint, UNDEFINED_ZONE_ID);

 updateEnrollState(endpoint, false);  // enrolled?

}

// If you leave the network, unenroll yourself.
void emberAfPluginIasZoneServerStackStatusCallback(EmberStatus status)
{
  uint8_t endpoint;
  uint8_t networkIndex;
  uint8_t i;

  // If the device has left the network, unenroll all endpoints on the device
  // that are servers of the IAS Zone Cluster
  if (status == EMBER_NETWORK_DOWN
      && emberAfNetworkState() == EMBER_NO_NETWORK) {
    for (i = 0; i < emberAfEndpointCount(); i++) {
      endpoint = emberAfEndpointFromIndex(i);
      networkIndex = emberAfNetworkIndexFromEndpointIndex(i);
      if (networkIndex == emberGetCurrentNetwork()
          && emberAfContainsServer(endpoint, ZCL_IAS_ZONE_CLUSTER_ID)) {
        unenrollSecurityDevice(endpoint);
      }
    }
  }
}
