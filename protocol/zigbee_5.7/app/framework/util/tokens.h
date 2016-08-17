// *******************************************************************
// * tokens.h
// *
// * Framework header file, responsible for including generated tokens
// *
// * Copyright 2014 Silicon Laboratories, Inc.                              *80*
// *******************************************************************

#ifdef CUSTOM_TOKEN_HEADER
#include CUSTOM_TOKEN_HEADER
#endif

#include "app/framework/include/af-types.h"

#ifdef GENERATED_TOKEN_HEADER

// If we have generated header, use it.
#include GENERATED_TOKEN_HEADER

#else 

// We don't have generated header. Default is to have no tokens.

#define GENERATED_TOKEN_LOADER(endpoint) {}
#define GENERATED_TOKEN_SAVER {}

#endif // GENERATED_TOKEN_HEADER


// These are the tokens for the scenes table if they are needed
#ifdef EMBER_AF_PLUGIN_SCENES_USE_TOKENS
  #define CREATOR_SCENES_NUM_ENTRIES 0x8723
  #define CREATOR_SCENES_TABLE 0x8724
  #ifdef DEFINETOKENS
    DEFINE_BASIC_TOKEN(SCENES_NUM_ENTRIES, uint8_t, 0x00)
    DEFINE_INDEXED_TOKEN(SCENES_TABLE,
                         EmberAfSceneTableEntry,
                         EMBER_AF_PLUGIN_SCENES_TABLE_SIZE,
                         {EMBER_AF_SCENE_TABLE_UNUSED_ENDPOINT_ID})
  #endif //DEFINETOKENS
#endif //EMBER_AF_PLUGIN_SCENES_USE_TOKEN

// These are the tokens for the report table if they are needed
#ifdef EMBER_AF_PLUGIN_REPORTING
  #define CREATOR_REPORT_TABLE 0x8725
  #ifdef DEFINETOKENS
    DEFINE_INDEXED_TOKEN(REPORT_TABLE,
                         EmberAfPluginReportingEntry,
                         EMBER_AF_PLUGIN_REPORTING_TABLE_SIZE,
                         {EMBER_ZCL_REPORTING_DIRECTION_REPORTED,
                          EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID})
  #endif //DEFINETOKENS
#endif //EMBER_AF_PLUGIN_REPORTING

#ifdef EMBER_AF_PLUGIN_RF4CE_GDP
  #include "app/framework/plugin/rf4ce-gdp/rf4ce-gdp-tokens.h"
#endif

#ifdef EMBER_AF_PLUGIN_RF4CE_ZRC20
  #include "app/framework/plugin/rf4ce-zrc20/rf4ce-zrc20-tokens.h"
#endif

#ifdef EMBER_AF_PLUGIN_RF4CE_MSO
  #include "app/framework/plugin/rf4ce-mso/rf4ce-mso-tokens.h"
#endif

#ifdef EMBER_AF_PLUGIN_LWIP
  #include "app/framework/plugin/lwip/lwip-tokens.h"
#endif

#ifdef EMBER_AF_PLUGIN_RF4CE_ZRC20_HA_SERVER
  #include "app/framework/plugin/rf4ce-zrc20-ha-server/rf4ce-zrc20-ha-server-tokens.h"
#endif

#ifdef EMBER_AF_PLUGIN_MFG_LIB
  #include "app/framework/plugin/mfg-lib/mfg-lib-tokens.h"
#endif

#ifdef EMBER_AF_PLUGIN_MANUFACTURING_LIBRARY
  #include "app/framework/plugin-soc/manufacturing-library-cli/manufacturing-library-cli-tokens.h"
#endif

#ifdef EMBER_AF_PLUGIN_BULB_UI
  #include "app/framework/plugin-soc/bulb-ui/bulb-ui-tokens.h"
#endif

#ifdef EMBER_AF_PLUGIN_CONNECTION_MANAGER
  #include "app/framework/plugin-soc/connection-manager/connection-manager-tokens.h"
#endif

#ifdef EMBER_AF_PLUGIN_CONFIGURATION_SERVER
   #include "app/framework/plugin-soc/configuration-server/configuration-server-tokens.h"
#endif

#ifdef EMBER_AF_PLUGIN_ILLUMINANCE_MEASUREMENT_SERVER
  #include "app/framework/plugin-soc/illuminance-measurement-server/illuminance-measurement-server-tokens.h"
#endif
