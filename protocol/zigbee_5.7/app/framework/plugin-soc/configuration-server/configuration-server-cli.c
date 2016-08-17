// Copyright 2016 Silicon Laboratories, Inc.                                *80*

// this file contains all the common includes for clusters in the util
#include "app/framework/include/af.h"
#include "app/framework/util/attribute-storage.h"

#define MAX_TOKEN_LENGTH OTA_CONFIG_MAX_TOKEN_LENGTH

bool emAfPluginConfigurationServerLocked(void);

void emAfConfigurationServerLockCommand(void)
{
  uint8_t lockTokenWrite = (uint8_t) emberUnsignedCommandArgument(0);

  halCommonSetToken(TOKEN_OTA_CONFIG_LOCK, (int8u *) (&lockTokenWrite));
}

void emAfConfigurationServerSetCommand(void)
{
  uint8_t tokenData[MAX_TOKEN_LENGTH + 1];
  uint16_t creator = (uint16_t)emberUnsignedCommandArgument(0);

  emberCopyStringArgument(1,
                          tokenData + 1,
                          MAX_TOKEN_LENGTH,
                          FALSE);
  
  tokenData[0] = 0xff;

  // Don't want to write stack tokens.  Also, don't want to write if this
  // has been locked down.
  if (emAfPluginConfigurationServerLocked() || creator >= 0x8000) {
    return;
  }

  emberAfOtaConfigurationClusterSetTokenCallback(creator, tokenData);

}

void emAfConfigurationServerReadCommand(void)
{
  uint16_t unsignedData16;
  uint8_t  unsignedData8;
  int8_t   signedData8;
  uint8_t tokenData[MAX_TOKEN_LENGTH + 1];

  halCommonGetToken((uint8_t *) &unsignedData8, TOKEN_OTA_CONFIG_LOCK);
  emberAfCorePrintln("LOCK %2x %d", 
                     CREATOR_OTA_CONFIG_LOCK, 
                     unsignedData8);
  halCommonGetToken((uint8_t *) &signedData8, TOKEN_OTA_CONFIG_TX_POWER);
  emberAfCorePrintln("TX_POWER %2x %d", 
                     CREATOR_OTA_CONFIG_TX_POWER,
                     signedData8);
  halCommonGetToken((uint8_t *) &signedData8, TOKEN_OTA_CONFIG_TX_POWER25);
  emberAfCorePrintln("TX_POWER_25 %2x %d", CREATOR_OTA_CONFIG_TX_POWER25, signedData8);
  halCommonGetToken((uint8_t *) &signedData8, TOKEN_OTA_CONFIG_TX_POWER26);
  emberAfCorePrintln("TX_POWER_26 %2x %d",
                     CREATOR_OTA_CONFIG_TX_POWER26,
                     signedData8);
  halCommonGetToken((uint8_t *) tokenData, TOKEN_OTA_CONFIG_MODEL_NAME);
  emberAfCorePrintln("MODEL_NAME %2x %s",
                     CREATOR_OTA_CONFIG_MODEL_NAME,
                     tokenData);
  halCommonGetToken((uint8_t *) tokenData, TOKEN_OTA_CONFIG_MANUFACTURER_NAME);
  emberAfCorePrintln("MANUFACTURER_NAME %2x %s",
                     CREATOR_OTA_CONFIG_MANUFACTURER_NAME,
                     tokenData);
  halCommonGetToken((uint8_t *) &unsignedData8, TOKEN_OTA_CONFIG_HW_VERSION);
  emberAfCorePrintln("HW_VERSION %2x %d", 
                     CREATOR_OTA_CONFIG_HW_VERSION,
                     unsignedData8);
  halCommonGetToken((uint8_t *) &unsignedData16, TOKEN_BULB_PWM_FREQUENCY_HZ);
  emberAfCorePrintln("PWM FREQUENCY %2x %d",
                     CREATOR_BULB_PWM_FREQUENCY_HZ,
                     unsignedData16);
  halCommonGetToken((uint8_t *) &unsignedData16, TOKEN_BULB_PWM_MIN_ON_US);
  emberAfCorePrintln("PWM MIN_ON %2x %d",
                     CREATOR_BULB_PWM_MIN_ON_US,
                     unsignedData16);
  halCommonGetToken((uint8_t *) &unsignedData16, TOKEN_BULB_PWM_MAX_ON_US);
  emberAfCorePrintln("PWM MAX_ON %2x %d",
                     CREATOR_BULB_PWM_MAX_ON_US,
                     unsignedData16);
  halCommonGetToken((uint8_t *) &unsignedData16, TOKEN_BULB_UI_MIN_ON_TIME);
  emberAfCorePrintln("UI MIN ON %2x %d",
                     CREATOR_BULB_UI_MIN_ON_TIME,
                     unsignedData16);
  halCommonGetToken((uint8_t *) &unsignedData16, TOKEN_BULB_UI_TIMEOUT);
  emberAfCorePrintln("UI MAX ON %2x %d",
                     CREATOR_BULB_UI_TIMEOUT,
                     unsignedData16);
  halCommonGetToken( (uint8_t *) &unsignedData8,
                     TOKEN_BULB_UI_POWER_UP_BEHAVIOR);  
  emberAfCorePrintln("UI POWER UP BEHAVIOR %2x %d",
                     CREATOR_BULB_UI_POWER_UP_BEHAVIOR,
                     unsignedData8);
}

