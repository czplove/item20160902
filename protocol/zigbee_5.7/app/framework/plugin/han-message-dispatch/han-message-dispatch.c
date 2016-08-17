// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#include "app/framework/include/af.h"
#include "app/framework/util/common.h"
#include "han-message-dispatch.h"
#include "security-manager.h"
#include "tunnel-manager.h"

static uint8_t esmeTunnelingServerEndpoint = EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_ESME_TUNNEL_SERVER_ENDPOINT;
static uint8_t commsHubTunnelingClientEndpoint = EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_COMMSHUB_TUNNEL_CLIENT_ENDPOINT;
static uint8_t tunnelIndex = EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX;

void emberAfPluginHanMessageDispatchInitCallback(void)
{
  emAfHanMessageDispatchTunnelInit();
}

bool emberAfKeyEstablishmentCallback(EmberAfKeyEstablishmentNotifyMessage status,
                                     boolean amInitiator,
                                     EmberNodeId partnerShortId,
                                     uint8_t delayInSeconds)
{
  if (status == LINK_KEY_ESTABLISHED) {
    tunnelIndex = emAfHanMessageDispatchTunnelCreate(partnerShortId,
                                                     EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_COMMSHUB_TUNNEL_CLIENT_ENDPOINT,
                                                     EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_ESME_TUNNEL_SERVER_ENDPOINT);
  }

  // Always allow key establishment to continue.
  return true;
}

bool emberAfPluginHanMessageDispatchSend(EmberEUI64 esme,
                                         uint16_t length,
                                         uint8_t *payload)
{
  EmberEUI64 currentNodeEui;
  boolean permission;
  EmberAfStatus status;

  emberAfGetEui64(currentNodeEui);
  permission = emberAfSecurityManagerRequestPermission(currentNodeEui,
                                                       commsHubTunnelingClientEndpoint,
                                                       esme,
                                                       esmeTunnelingServerEndpoint);

  if (!permission) {
    emberAfAppPrintln("Unable to send message due to permission issue.");
    return false;
  }

  status = emAfHanMessageDispatchTunnelSendData(tunnelIndex,
                                                payload,
                                                length);
  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfAppPrintln("Unable to send message due to tunnel error: 0x%x", status);
    return false;
  }

  return true;
}

 
void emAfPluginHanMessageDispatchTunnelDataReceivedCallback(uint8_t tunnelIndex,
                                                            uint8_t *payload,
                                                            uint16_t length)
{
  emberAfPluginHanMessageDispatchDataReceivedCallback(payload, length);
}


void emAfPluginHanMessageDispatchTunnelDataErrorCallback(int8u tunnelIndex)
{
  emberAfAppPrintln("Unable to send message.");
}
