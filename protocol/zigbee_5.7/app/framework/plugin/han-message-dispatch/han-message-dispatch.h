// Copyright 2016 Silicon Laboratories, Inc.                                *80*

/**
 * @brief Send data to a device through a tunnel.
 *
 * This function can be used to transfer data to a device through a tunnel.
 *
 * @param dst The EUI64 of the destination device
 * @param length The length in octets of the data.
 * @param payload Buffer containing the raw octets of the data.
 * @return TRUE if the data was succesfully sent, FALSE otherwise.
 */
bool emberAfPluginHanMessageDispatchSend(EmberEUI64 dst,
                                         uint16_t length, 
                                         uint8_t *payload);
