// *******************************************************************
// * ota-server.h
// *
// *
// * Copyright 2010 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

uint8_t emAfOtaServerGetBlockSize(void);
uint8_t emAfOtaImageBlockRequestHandler(EmberAfImageBlockRequestCallbackStruct* callbackData);

bool emAfOtaPageRequestErrorHandler(void);

void emAfOtaPageRequestTick(uint8_t endpoint);

// Returns the status code to the request.
uint8_t emAfOtaPageRequestHandler(uint8_t clientEndpoint,
                                uint8_t serverEndpoint,
                                const EmberAfOtaImageId* id,
                                uint32_t offset,
                                uint8_t maxDataSize,
                                uint16_t pageSize,
                                uint16_t responseSpacing);

bool emAfOtaServerHandlingPageRequest(void);

// This will eventually be moved into a Plugin specific callbacks file.
void emberAfOtaServerSendUpgradeCommandCallback(EmberNodeId dest,
                                                uint8_t endpoint,
                                                const EmberAfOtaImageId* id);

/** @brief OTA Server Block Sent Callback
 *
 * This function will be called when a block is sent to a device
 */
void emberAfPluginOtaServerBlockSentCallback(int8u actualLength,int16u manufacturerId,int16u imageTypeId,int32u firmwareVersion);

/** @brief OTA Server Update Started Callback
 *
 * This function will be called when an update has started
 */
void emberAfPluginOtaServerUpdateStartedCallback(int16u mid,int16u itype,int32u fwv,int8u maxDataSize,int32u offset);

/** @brief OTA Server Update Failed callback
 *
 * This function will be called when an update has failed
 */
void emberAfPluginOtaServerImageFailedCallback(); 

/** @brief OTA Server Finished Callback
 *
 * This function will be called when an OTA update has finished
 */
void emberAfPluginOtaServerFinishedCallback(int16u mid,int16u itype,int32u fwv, EmberNodeId source, int8u status);

#if defined(EMBER_TEST) && !defined(EM_AF_TEST_HARNESS_CODE)
  #define EM_AF_TEST_HARNESS_CODE
#endif

