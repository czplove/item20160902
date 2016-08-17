// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "app/framework/plugin/tunneling-client/tunneling-client.h"
#include "han-message-dispatch.h"
#include "tunnel-manager.h"

/*
 * The Tunnel Manager is responsible for establishing and maintaining tunnels
 * to all devices.  There are four APIs exposed by the tunnel manager. The
 * init function is called at startup and initializes internal data structures.
 * The create function is called after the CBKE with the device, the sendData
 * function is called whenever data is to be sent to the device, and the
 * destroy function is called whenever the tunnel to the device should
 * be torn down. There are also 2 callbacks that the tunnel manager will call.
 * They are emAfPluginHanMessageDispatchTunnelDataReceivedCallback which is
 * called when data is received from a tunnel and
 * emAfPluginHanMessageDispatchTunnelDataErrorCallback which is called when
 * an error occurs while trying the send data over a tunnel.
 *
 * There can only be one tunnel between a specific client endpoint
 * and server endpoint.
 */
typedef enum {
  UNUSED_TUNNEL,
  PENDING_TUNNEL,
  ACTIVE_TUNNEL,
  CLOSED_TUNNEL
} EmAfHanMessageDispatchTunnelState;

typedef struct {
  EmAfHanMessageDispatchTunnelState state;
  EmberNodeId server;
  uint8_t clientEndpoint;
  uint8_t serverEndpoint;
  uint8_t clientTunnelId;
} EmAfHanMessageDispatchTunnel;

#define EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_TUNNEL_LIMIT EMBER_AF_PLUGIN_TUNNELING_CLIENT_TUNNEL_LIMIT
static EmAfHanMessageDispatchTunnel tunnels[EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_TUNNEL_LIMIT];
static uint8_t pendingIndex = EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX;

/*
 * Per section 10.2.2 of the GBCS version 0.8
 *
 * "Devices shall set the value of the ManufacturerCode field in any
 * RequestTunnel command to 0xFFFF (‘not used’).
 *
 * The ProtocolID of all Remote Party Messages shall be 6 (‘GB-HGRP’). Devices
 * shall set the value of the ProtocolID field in any RequestTunnel command to 6.
 *
 * Devices shall set the value of the FlowControlSupport field in any
 * RequestTunnel command to ‘False’.
 */
#define GBCS_TUNNELING_MANUFACTURER_CODE      0xFFFF
#define GBCS_TUNNELING_PROTOCOL_ID            0x06
#define GBCS_TUNNELING_FLOW_CONTROL_SUPPORT   false

EmberEventControl emberAfPluginHanMessageDispatchTunnelEventControl;

//------------------------------------------------------------------------------
// Forward Declarations
static bool createTunnel(uint8_t tunnelIndex);
static bool handleTunnelOpenFailure(uint8_t tunnelIndex,
                                       EmberAfPluginTunnelingClientStatus status);
static uint8_t findTunnel(EmberNodeId server,
                        uint8_t clientEndpoint,
                        uint8_t serverEndpoint);
static uint8_t findTunnelByClientTunnelId(uint8_t clientTunnelId);

//------------------------------------------------------------------------------

// This should be called from the HAN Message Dispatch Init callback.
void emAfHanMessageDispatchTunnelInit(void)
{
  uint8_t i;
  for (i = 0; i < EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_TUNNEL_LIMIT; i++) {
    tunnels[i].state = UNUSED_TUNNEL;
  }
}

// This should be called after CBKE
uint8_t emAfHanMessageDispatchTunnelCreate(EmberNodeId server,
                                           uint8_t clientEndpoint,
                                           uint8_t serverEndpoint)
{
  // We only support one tunnel to the specified server/endpoint per
  // client endpoint so if we already have a tunnel we're done..
  uint8_t tunnelIndex = findTunnel(server, clientEndpoint, serverEndpoint);
  if (tunnelIndex != EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX) {
    return tunnelIndex;
  }

  // Find a slot in the tunnels table for the new tunnel
  for (tunnelIndex = 0; 
       tunnelIndex < EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_TUNNEL_LIMIT; 
       tunnelIndex++) {
    if (tunnels[tunnelIndex].state == UNUSED_TUNNEL) {
      tunnels[tunnelIndex].server = server;
      tunnels[tunnelIndex].clientEndpoint = clientEndpoint;
      tunnels[tunnelIndex].serverEndpoint = serverEndpoint;
      tunnels[tunnelIndex].clientTunnelId = 
        EMBER_AF_PLUGIN_TUNNELING_CLIENT_NULL_INDEX;
      if (!createTunnel(tunnelIndex)) {
        return EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX;
      }

      // If we made it here, the tunnel is in the process of being created.
      return tunnelIndex;
    }
  }

  // This is a misconfiguration or a bug in the code calling this API. Either
  // the tunnel client plugin limit is set too low for the number of tunnels
  // required or the code that is calling this function in error.  Either way,
  // we'll print the error and return false indicating that the tunnel was
  // not created.
  emberAfAppPrintln("%p%p%p",
                    "Error: ",
                    "Tunnel Create failed: ",
                    "Too many tunnels");
  return EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX;
}

EmberAfStatus emAfHanMessageDispatchTunnelSendData(uint8_t tunnelIndex,
                                                   uint8_t *data,
                                                   uint16_t dataLen)
{
  EmberAfStatus status = EMBER_ZCL_STATUS_NOT_FOUND;
  if (tunnelIndex < EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_TUNNEL_LIMIT) {
    if (tunnels[tunnelIndex].state == ACTIVE_TUNNEL) {
      status = 
        emberAfPluginTunnelingClientTransferData(
          tunnels[tunnelIndex].clientTunnelId,
          data,
          dataLen);
      if (status == EMBER_ZCL_STATUS_NOT_FOUND) {
        // For some reason the tunnel has gone away.
        createTunnel(tunnelIndex);
      }
    } else if (tunnels[tunnelIndex].state == PENDING_TUNNEL) {
    } else if (tunnels[tunnelIndex].state == CLOSED_TUNNEL) {
      createTunnel(tunnelIndex);
    }
  }
  return status;
}

EmberAfStatus emAfHanMessageDispatchTunnelDestroy(uint8_t tunnelIndex)
{
  EmberAfStatus status = EMBER_ZCL_STATUS_NOT_FOUND;
  if (tunnelIndex < EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_TUNNEL_LIMIT
      && tunnels[tunnelIndex].state != UNUSED_TUNNEL) {
    status = 
      emberAfPluginTunnelingClientCloseTunnel(
        tunnels[tunnelIndex].clientTunnelId);
    if (status == EMBER_ZCL_STATUS_SUCCESS) {
      tunnels[tunnelIndex].state = UNUSED_TUNNEL;
      if (tunnelIndex == pendingIndex) {
        pendingIndex = EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX;
      }
    }
  }
  return status;
}

//------------------------------------------------------------------------------
// Callbacks
void emberAfPluginHanMessageDispatchTunnelEventHandler(void)
{
  emberEventControlSetInactive(
    emberAfPluginHanMessageDispatchTunnelEventControl);
  if (pendingIndex != EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX) {
    uint8_t tunnelIndex = pendingIndex;
    pendingIndex = EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX;
    createTunnel(tunnelIndex);
  }
}

/** @brief Tunnel Opened
 *
 * This function is called by the Tunneling client plugin whenever a tunnel is
 * opened.
 *
 * @param clientTunnelId The ID of the tunnel that has been opened.  Ver.:
 * always
 * @param tunnelStatus The status of the request.  Ver.: always
 * @param maximumIncomingTransferSize The maximum incoming transfer size of
 * the server.  Ver.: always
 */
void emberAfPluginTunnelingClientTunnelOpenedCallback(
       uint8_t clientTunnelId,
       EmberAfPluginTunnelingClientStatus tunnelStatus,
       uint16_t maximumIncomingTransferSize) 
{
  emberAfAppPrintln("ClientTunnelOpened:0x%x,0x%x,0x%2x",
                    clientTunnelId,
                    tunnelStatus,
                    maximumIncomingTransferSize);
  if (pendingIndex != EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX) {
    uint8_t tunnelIndex = pendingIndex;
    pendingIndex = EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX;
    if (tunnelStatus == EMBER_AF_PLUGIN_TUNNELING_CLIENT_SUCCESS) {
      tunnels[tunnelIndex].clientTunnelId = clientTunnelId;
      tunnels[tunnelIndex].state = ACTIVE_TUNNEL;
      return;
    }

    // see if we can recover from the open failure
    if (handleTunnelOpenFailure(tunnelIndex, tunnelStatus)) {
      pendingIndex = tunnelIndex;
      return;
    }
  }
}

/** @brief Data Received
 *
 * This function is called by the Tunneling client plugin whenever data is
 * received from a server through a tunnel.
 *
 * @param clientTunnelId The id of the tunnel through which the data was
 * received.  Ver.: always
 * @param data Buffer containing the raw octets of the data.  Ver.: always
 * @param dataLen The length in octets of the data.  Ver.: always
 */
void emberAfPluginTunnelingClientDataReceivedCallback(uint8_t clientTunnelId,
                                                      uint8_t *data,
                                                      uint16_t dataLen)
{
  emberAfAppPrintln("ClientDataReceived:0x%x,0x%2x", clientTunnelId, dataLen);
  uint8_t tunnelIndex = findTunnelByClientTunnelId(clientTunnelId);
  if (tunnelIndex != EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX) {
    emAfPluginHanMessageDispatchTunnelDataReceivedCallback(tunnelIndex,
                                                           data,
                                                           dataLen);
  }
}

/** @brief Data Error
 *
 * This function is called by the Tunneling client plugin whenever a data
 * error occurs on a tunnel.  Errors occur if a device attempts to send data
 * on tunnel that is no longer active or if the tunneling does not belong to
 * the device.
 *
 * @param clientTunnelId The ID of the tunnel on which this data error
 * occurred.  Ver.: always
 * @param transferDataStatus The error that occurred.  Ver.: always
 */
void emberAfPluginTunnelingClientDataErrorCallback(
       uint8_t clientTunnelId,
       EmberAfTunnelingTransferDataStatus transferDataStatus) 
{
  emberAfAppPrintln("ClientDataError:0x%x,0x%x", 
                    clientTunnelId, 
                    transferDataStatus);
  uint8_t tunnelIndex = findTunnelByClientTunnelId(clientTunnelId);
  if (tunnelIndex != EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX) {
    if (transferDataStatus 
        == EMBER_ZCL_TUNNELING_TRANSFER_DATA_STATUS_NO_SUCH_TUNNEL) {
      createTunnel(tunnelIndex);
    }
    emAfPluginHanMessageDispatchTunnelDataErrorCallback(tunnelIndex);
  }
}

/** @brief Tunnel Closed
 *
 * This function is called by the Tunneling client plugin whenever a server
 * sends a notification that it preemptively closed an inactive tunnel.
 * Servers are not required to notify clients of tunnel closures, so
 * applications cannot rely on this callback being called for all tunnels.
 *
 * @param clientTunnelId The ID of the tunnel that has been closed.  Ver.:
 * always
 */
void emberAfPluginTunnelingClientTunnelClosedCallback(uint8_t clientTunnelId) 
{
  emberAfAppPrintln("ClientTunnelClosed:0x%x", clientTunnelId);
  uint8_t tunnelIndex = findTunnelByClientTunnelId(clientTunnelId);
  if (tunnelIndex != EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX) {
    tunnels[tunnelIndex].state = CLOSED_TUNNEL;
    if (tunnelIndex == pendingIndex) {
      pendingIndex = EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX;
    }
  }
}

//------------------------------------------------------------------------------
// Internal Functions
static bool createTunnel(uint8_t tunnelIndex)
{
  EmberAfPluginTunnelingClientStatus status;

  if (pendingIndex != EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX) {
    emberAfAppPrintln("%p%p%p",
                      "Error: ",
                      "Tunnel Create failed: ",
                      "Simultaneous create requests");
    return false;
  }
  pendingIndex = tunnelIndex;

  status = emberAfPluginTunnelingClientRequestTunnel(
             tunnels[tunnelIndex].server,
             tunnels[tunnelIndex].clientEndpoint,
             tunnels[tunnelIndex].serverEndpoint,
             GBCS_TUNNELING_PROTOCOL_ID,
             GBCS_TUNNELING_MANUFACTURER_CODE,
             GBCS_TUNNELING_FLOW_CONTROL_SUPPORT);
  if (status != EMBER_AF_PLUGIN_TUNNELING_CLIENT_SUCCESS
      && !handleTunnelOpenFailure(tunnelIndex, status)) {
    pendingIndex = EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX;
    return false;
  }

  tunnels[tunnelIndex].state = PENDING_TUNNEL;
  return true;
}

// See if we can recover from tunnel creation issues.
static bool handleTunnelOpenFailure(uint8_t tunnelIndex, 
                                    EmberAfPluginTunnelingClientStatus status)
{
  uint8_t i;

  if (status == EMBER_AF_PLUGIN_TUNNELING_CLIENT_BUSY) {
    // Per GBCS send another request 3 minutes from now
    emberEventControlSetDelayMinutes(
      emberAfPluginHanMessageDispatchTunnelEventControl, 
      3);
    emberEventControlSetActive(
      emberAfPluginHanMessageDispatchTunnelEventControl);
    return true;
  } else if (status == EMBER_AF_PLUGIN_TUNNELING_CLIENT_NO_MORE_TUNNEL_IDS) {
    // Per GBCS close any other tunnels we may have with the device
    // and once all responses are received try the RequestTunnel again
    bool retryRequest = false;
    for (i = 0; i < EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_TUNNEL_LIMIT; i++) {
      if (i != tunnelIndex 
          && tunnels[i].server == tunnels[tunnelIndex].server) {
        retryRequest = true;
        emAfHanMessageDispatchTunnelDestroy(i);
      }
    }
    if (retryRequest) {
      // We'll retry the request but not immediately so as to give the tunnel(s)
      // a chance to clean up.  We'll wait 5 seconds before trying again.
      emberEventControlSetDelayQS(
        emberAfPluginHanMessageDispatchTunnelEventControl, 20);
      emberEventControlSetActive(
        emberAfPluginHanMessageDispatchTunnelEventControl);
      return true;
    }
    // no tunnels were closed so nothing more we can do
    emberAfAppPrintln("%p%p%p",
                      "Error: ",
                      "Tunnel Create failed: ",
                      "No more tunnel ids");
    return false;
  }

  // All other errors are either due to mis-configuration or errors that we
  // cannot recover from so print the error and return false.
  emberAfAppPrintln("%p%p%p0x%x",
                    "Error: ",
                    "Tunnel Create failed: ",
                    "Tunneling Client Status: ",
                    status);
  return false;
}

// Find an active tunnel for the given server nodeId and client and server endpoints.
static uint8_t findTunnel(EmberNodeId server,
                          uint8_t clientEndpoint,
                          uint8_t serverEndpoint)
{
  uint8_t tunnelIndex;
  for (tunnelIndex = 0; 
       tunnelIndex < EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_TUNNEL_LIMIT; 
       tunnelIndex++) {
    if (tunnels[tunnelIndex].state != UNUSED_TUNNEL
        && tunnels[tunnelIndex].server == server
        && tunnels[tunnelIndex].clientEndpoint == clientEndpoint
        && tunnels[tunnelIndex].serverEndpoint == serverEndpoint) {
      return tunnelIndex;
    }
  }
  return EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX;
}

// Find an active tunnel from the given client tunneling plugin identifer,
static uint8_t findTunnelByClientTunnelId(uint8_t clientTunnelId)
{
  uint8_t tunnelIndex;
  for (tunnelIndex = 0; 
       tunnelIndex < EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_TUNNEL_LIMIT; 
       tunnelIndex++) {
    if (tunnels[tunnelIndex].state != UNUSED_TUNNEL
        && tunnels[tunnelIndex].clientTunnelId == clientTunnelId) {
      return tunnelIndex;
    }
  }
  return EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX;
}
