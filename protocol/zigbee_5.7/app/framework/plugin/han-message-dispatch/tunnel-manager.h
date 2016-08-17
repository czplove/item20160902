// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#ifndef TUNNEL_MANAGER_H_
#define TUNNEL_MANAGER_H_

#define EM_AF_PLUGIN_HAN_MESSAGE_DISPATCH_NULL_TUNNEL_INDEX 0xFF

/**
 * @brief Initialize internal data structures.
 *
 * This function should be called from the plugin init callback.
 */
void emAfHanMessageDispatchTunnelInit(void);

/**
 * @brief Create a tunnel to a device of type ESME, HCALCS or PPMID.
 *
 * As defined in section 10.2.2.1 of the GBCS version 0.8 and copied below the
 * CHF will call this function to request a tunnel be created with the specified
 * destination.
 *
 * "When a Communications Hub has successfully established a shared secret key
 * using CBKE with a Device of type ESME, HCALCS or PPMID, the CHF shall send a
 * RequestTunnel command to the Device to request a tunnel association with the
 * Device.
 *
 * Where an ESME, a HCALCS or a PPMID remains in the CHF Device Log, the CHF
 * shall send a RequestTunnel command to the Device whenever:
 *  -  0xFFFF seconds have elapsed since receipt of the most recent
 *     RequestTunnelResponse command from that Device; or
 *  -  the CHF receives a Remote Party Message addressed to the Device but does
 *     not have a functioning tunnel association with the Device; or
 *  -  the CHF powers on.
 *
 *  Where the CHF receives a RequestTunnelResponse command from a Device with a
 *  TunnelStatus of 0x01 (Busy), the CHF shall send another RequestTunnel
 *  command three minutes later.
 *
 *  Where the CHF receives a RequestTunnelResponse command from a Device with a
 *  TunnelStatus of 0x02 (No More Tunnel IDs), the CHF shall send a CloseTunnel
 *  command for any TunnelID that may relate to an active tunnel association
 *  with that Device and, after receiving responses to all such commands, send
 *  another RequestTunnel command."
 *
 * @param server The network address of the server to which the request will be
 * sent.
 * @param clientEndpoint The local endpoint from which the request will be
 * sent.
 * @param serverEndpoint The remote endpoint to which the request will be sent.
 * @return a tunnel index or
 * ::EMBER_AF_PLUGIN_HAN_MESSAGE_DISPATCH_TUNNEL_NULL_TUNNEL_INDEX if an error occurred
 */
uint8_t emAfHanMessageDispatchTunnelCreate(EmberNodeId server,
                                           uint8_t clientEndpoint,
                                           uint8_t serverEndpoint);

/**
 * @brief Transfer data to a server through a tunnel.
 *
 * This function can be used to transfer data to a server through a tunnel.
 *
 * @param tunnelIndex The index of the tunnel over which the data will be sent.
 * @param data Buffer containing the raw octets of the data.
 * @param dataLen The length in octets of the data.
 * @return ::EMBER_ZCL_STATUS_SUCCESS if the data was sent,
 * ::EMBER_ZCL_STATUS_FAILURE if an error occurred, or
 * ::EMBER_ZCL_STATUS_NOT_FOUND if the tunnel does not exist.
 */
EmberAfStatus emAfHanMessageDispatchTunnelSendData(uint8_t tunnelIndex,
                                                   uint8_t *data,
                                                   uint16_t dataLen);

/**
 * @brief Close a tunnel.
 *
 * This function can be used to close a tunnel.
 *
 * @param tunnelIndex The index of the tunnel to close.
 * @return ::EMBER_ZCL_STATUS_SUCCESS if the close request was sent,
 * ::EMBER_ZCL_STATUS_FAILURE if an error occurred, or
 * ::EMBER_ZCL_STATUS_NOT_FOUND if the tunnel does not exist.
 */
EmberAfStatus emAfHanMessageDispatchCloseTunnel(uint8_t tunnelIndex);

#endif /* TUNNEL_MANAGER_H_ */
