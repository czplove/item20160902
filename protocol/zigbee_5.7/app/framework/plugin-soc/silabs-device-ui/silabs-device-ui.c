// Copyright 2015 Silicon Laboratories, Inc.                                *80*

#ifdef EMBER_SCRIPTED_TEST
#include "app/framework/plugin-soc/connection-manager/connection-manager-test.h"
#endif

#include "app/framework/include/af.h"
#include "app/framework/util/attribute-storage.h"
#include "app/framework/plugin-soc/connection-manager/connection-manager.h"
#include "app/framework/plugin-soc/manufacturing-library-cli/manufacturing-library-cli-plugin.h"
#include "app/framework/plugin-soc/silabs-device-ui/silabs-device-ui.h"
#include "hal/micro/led-blink.h"

#ifdef EMBER_AF_PLUGIN_EZMODE_COMMISSIONING
#include "app/framework/plugin/ezmode-commissioning/ez-mode.h"
#endif //EMBER_AF_PLUGIN_EZMODE_COMMISSIONING

//------------------------------------------------------------------------------
// Plugin private macro definitions

#define DEBOUNCE_TIME_MS \
          EMBER_AF_PLUGIN_SILABS_DEVICE_UI_BUTTON_DEBOUNCE_TIME_MS
#define BUTTON_NETWORK_LEAVE_TIME_MS  1000
#define IDENTIFY_ENABLE_LENGTH_S      180

#define EZ_MODE_ROLE_IS_SERVER \
          EMBER_AF_PLUGIN_SILABS_DEVICE_UI_EZ_MODE_ROLE

// These are default values to modify the UI LED's blink pattern for network
// join and network leave.
#define LED_LOST_ON_TIME_MS          250
#define LED_LOST_OFF_TIME_MS         750
#define LED_BLINK_ON_TIME_MS         200
#define LED_PA_REJOIN_ON_TIME_MS     250
#define LED_SEARCH_BLINK_OFF_TIME_MS 1800
#define LED_FOUND_BLINK_OFF_TIME_MS  250
#define LED_FOUND_BLINK_ON_TIME_MS   250
#define LED_PA_REJOIN_OFF1_TIME_MS   250
#define LED_PA_REJOIN_OFF2_TIME_MS   750
#define LED_IDENTIFY_ON_TIME_MS      250
#define LED_IDENTIFY_OFF1_TIME_MS    250
#define LED_IDENTIFY_OFF2_TIME_MS    1250
#define DEFAULT_NUM_SEARCH_BLINKS    100
#define DEFAULT_NUM_IDENTIFY_BLINKS  100
#define DEFAULT_NUM_PA_REJOIN_BLINKS 100

//------------------------------------------------------------------------------
// Plugin events
EmberEventControl emberAfPluginSilabsDeviceUiButtonPressCountEventControl;
EmberEventControl emberAfPluginSilabsDeviceUiInitEventControl;

//------------------------------------------------------------------------------
// plugin private global variables

// State variables for controlling LED blink behavior on network join/leave
static uint16_t networkLostBlinkPattern[] =
  { LED_LOST_ON_TIME_MS, LED_LOST_OFF_TIME_MS };
static uint16_t networkSearchBlinkPattern[] =
  { LED_BLINK_ON_TIME_MS, LED_SEARCH_BLINK_OFF_TIME_MS };
static uint16_t networkFoundBlinkPattern[] =
  { LED_FOUND_BLINK_ON_TIME_MS, LED_FOUND_BLINK_OFF_TIME_MS };
static uint16_t networkIdentifyBlinkPattern[] =
  { LED_IDENTIFY_ON_TIME_MS, LED_IDENTIFY_OFF1_TIME_MS,
    LED_IDENTIFY_ON_TIME_MS, LED_IDENTIFY_OFF2_TIME_MS };
static uint16_t networkProactiveRejoinBlinkPattern[] =
  { LED_PA_REJOIN_ON_TIME_MS, LED_PA_REJOIN_OFF1_TIME_MS,
    LED_PA_REJOIN_ON_TIME_MS, LED_PA_REJOIN_OFF1_TIME_MS,
    LED_PA_REJOIN_ON_TIME_MS, LED_PA_REJOIN_OFF2_TIME_MS };

static uint8_t consecutiveButtonPressCount;
#if defined(EMBER_AF_PLUGIN_EZMODE_COMMISSIONING) && !EZ_MODE_ROLE_IS_SERVER
static uint16_t ezModeClusterIds[EMBER_AF_PLUGIN_SILABS_DEVICE_UI_EZ_MODE_MAX_CLUSTERS] = {0};
static uint8_t ezModeClusterIdsLength = 0;
#endif

//------------------------------------------------------------------------------
// plugin private function prototypes
static void enableIdentify(void);

//------------------------------------------------------------------------------
// Plugin consumed callback implementations

//******************************************************************************
// Init callback, executes sometime early in device boot chain.  This function
// will handle debug and UI LED control on startup, and schedule the reboot
// event to occur after the system finishes initializing.
//******************************************************************************
void emberAfPluginSilabsDeviceUiInitCallback(void)
{
  consecutiveButtonPressCount = 0;

  emberEventControlSetActive(emberAfPluginSilabsDeviceUiInitEventControl);
}

//******************************************************************************
// This callback will execute whenever the network stack has a significant
// change in state (network joined, network join failed, etc).  It is used to
// activate LED blinking patterns as follows:
//   When first joining a network, blink the "network found" pattern.
//   If the network goes down because the parent was lost, cease any current
//     blinking patterns.
//   If the network goes down because the device left the network, blink the
//     "newtork lost" pattern
//******************************************************************************
void emberAfPluginSilabsDeviceUiStackStatusCallback(EmberStatus status)
{
  if (status == EMBER_NETWORK_UP) {
    emberAfPluginSilabsDeviceUiLedNetworkFoundBlink();
  } else if (status == EMBER_NETWORK_DOWN
             && emberAfNetworkState() == EMBER_NO_NETWORK) {
    emberAfPluginSilabsDeviceUiLedNetworkLostBlink();
  }
}

void emberAfPluginButtonInterfaceButton0PressedShortCallback(
  uint16_t timePressedMs)
{
  // If the button was not held for longer than the debounce time, ignore the
  // press.
  if (timePressedMs < DEBOUNCE_TIME_MS) {
    return;
  }

  if (timePressedMs >= BUTTON_NETWORK_LEAVE_TIME_MS) {
    emberAfPluginConnectionManagerLeaveNetworkAndStartSearchForNewOne();
  } else {
    consecutiveButtonPressCount++;
    emberEventControlSetDelayMS(
      emberAfPluginSilabsDeviceUiButtonPressCountEventControl,
      EMBER_AF_PLUGIN_SILABS_DEVICE_UI_CONSECUTIVE_PRESS_TIMEOUT_MS);
  }
}

// The following code implements a fix for issue EMAPPFWKV2-1294 where the
// identify code fails to call the stop identifiy feedback callback after
// receiving a reset to factory defaults command during an identify operation.
void emberAfPluginBasicResetToFactoryDefaultsCallback(uint8_t endpoint)
{
  emberAfIdentifyClusterServerAttributeChangedCallback(endpoint,
                                                ZCL_IDENTIFY_TIME_ATTRIBUTE_ID);
}

// This is called when identify is started, either from an attribute write or a
// start identify command.  This should cause the device to blink the identify
// pattern
void emberAfPluginIdentifyStartFeedbackCallback(uint8_t endpoint,
                                                uint16_t identifyTime)
{
  emberAfAppPrintln("Beginning identify blink pattern");
  emberAfPluginSilabsDeviceUiLedIdentifyBlink();
}

// This is called when identify is stopped, either from an attribute write or a
// stop identify command.  This should cause the device to stop blinking its
// identify pattern.
void emberAfPluginIdentifyStopFeedbackCallback(uint8_t endpoint)
{
  emberAfAppPrintln("Identify has finished");
  halLedBlinkLedOff(0);
}

//------------------------------------------------------------------------------
// Plugin event handlers

//------------------------------------------------------------------------------
// Handle the buttonPressEvent going active.  This signals that the user has
// finished a series of short button presses on the UI button, so some sort of
// network activity should take place.
void emberAfPluginSilabsDeviceUiButtonPressCountEventHandler(void)
{
  uint8_t i;
  uint8_t endpoint;
  EmberStatus status;

  emberEventControlSetInactive(
    emberAfPluginSilabsDeviceUiButtonPressCountEventControl);

  if (emberAfNetworkState() != EMBER_NO_NETWORK) {
    // If on a network:
    // 2 presses activates identify
    // 3 presses blinks network status
    // 4 presses initiates a proactive rejoin
    // 5 presses sends an identify to anyone the device is controlling
    // 6 presses starts EZ Mode commissioning
    switch (consecutiveButtonPressCount) {
    case 2:
      enableIdentify();
      break;
    case 3:
      emberAfAppPrintln("Blinking user requested network status");
      emberAfPluginSilabsDeviceUiLedNetworkFoundBlink();
      break;
    case 4:
      emberAfPluginSilabsDeviceUiLedProactiveRejoinBlink();
      emberAfStartMoveCallback();
      break;
    case 5:
      for(i = 0; i < emberAfEndpointCount(); i++) {
        // When using emberAfSendCommandUnicastToBindings, the destination
        // endpoint will be automatically filled out, and the command will only
        // send if the source endpoint matches the source endpoint in the
        // binding table.
        endpoint = emberAfEndpointFromIndex(i);
        emberAfSetCommandEndpoints(endpoint, 0);
        emberAfFillCommandIdentifyClusterIdentify(IDENTIFY_ENABLE_LENGTH_S);
        status = emberAfSendCommandUnicastToBindings();
        
        if (status != EMBER_SUCCESS) {
          emberAfCorePrintln("Error 0x%X sending identify from endpoint %d",
                             status,
                             i);
        } else {
          emberAfCorePrintln("Sending identify from ep %d", i);
        }
      }
      break;
    case 6:
#ifdef EMBER_AF_PLUGIN_EZMODE_COMMISSIONING
#if EZ_MODE_ROLE_IS_SERVER
      emberAfEzmodeServerCommission(
        EMBER_AF_PLUGIN_SILABS_DEVICE_UI_EZ_MODE_ENDPOINT);
#else
      emberAfEzmodeClientCommission(
        EMBER_AF_PLUGIN_SILABS_DEVICE_UI_EZ_MODE_ENDPOINT,
        (EmberAfEzModeCommissioningDirection)EMBER_AF_PLUGIN_SILABS_DEVICE_UI_EZ_MODE_DIRECTION,
        (const uint16_t*)&ezModeClusterIds,
        ezModeClusterIdsLength);
#endif //EZ_MODE_ROLE_IS_SERVER
#endif //EMBER_AF_PLUGIN_EZMODE_COMMISSIONING
      break;
    default:
      break;
    }
  } else {
    // If not a network, then regardless of button presses or length, we want to
    // make sure we are looking for a network.
    emberAfPluginConnectionManagerResetJoinAttempts();
    if (!emberStackIsPerformingRejoin()) {
      emberAfPluginConnectionManagerLeaveNetworkAndStartSearchForNewOne();
    }
  }

  consecutiveButtonPressCount = 0;
}

//******************************************************************************
// Init event.  To be called sometime after all system init functions have
// executed.  This event is used to check the network state on startup and
// blink the appropriate pattern.
//******************************************************************************
void emberAfPluginSilabsDeviceUiInitEventHandler(void)
{
  emberEventControlSetInactive(emberAfPluginSilabsDeviceUiInitEventControl);

  if (emberAfNetworkState() == EMBER_NO_NETWORK) {
    emberAfPluginSilabsDeviceUiLedNetworkLostBlink();
  }
  else if (emberAfNetworkState() == EMBER_JOINED_NETWORK) {
    emberAfPluginSilabsDeviceUiLedNetworkFoundBlink();
  }
}

//------------------------------------------------------------------------------
// Plugin public API function implementations

EmberStatus emberAfPluginSilabsDeviceUiSetEzModeClusters(
  const EmberAfClusterId *clusterIds,
  uint8_t clusterIdsLength)
{
  EmberStatus returnStatus = EMBER_INVALID_CALL;
#if defined(EMBER_AF_PLUGIN_EZMODE_COMMISSIONING) && !EZ_MODE_ROLE_IS_SERVER
  uint8_t i;

  if (clusterIdsLength < EMBER_AF_PLUGIN_SILABS_DEVICE_UI_EZ_MODE_MAX_CLUSTERS) {
    ezModeClusterIdsLength = clusterIdsLength;
    returnStatus = EMBER_SUCCESS;
  } else {
    return EMBER_TABLE_FULL;
  }

  for (i = 0; i < ezModeClusterIdsLength; i++) {
    ezModeClusterIds[i] = clusterIds[i];
  }
#endif //EMBER_AF_PLUGIN_EZMODE_COMMISSIONING && !EZ_MODE_ROLE_IS_SERVER
  return returnStatus;
}

void emberAfPluginSilabsDeviceUiLedNetworkFoundBlink(void)
{
  halLedBlinkPattern(EMBER_AF_PLUGIN_SILABS_DEVICE_UI_NUM_JOIN_BLINKS,
                     COUNTOF(networkFoundBlinkPattern),
                     networkFoundBlinkPattern);
}

void emberAfPluginSilabsDeviceUiLedNetworkLostBlink(void)
{
  halLedBlinkPattern(EMBER_AF_PLUGIN_SILABS_DEVICE_UI_NUM_LEAVE_BLINKS,
                     COUNTOF(networkLostBlinkPattern),
                     networkLostBlinkPattern);
}

void emberAfPluginSilabsDeviceUiLedNetworkSearchingBlink(void)
{
  halLedBlinkPattern(DEFAULT_NUM_SEARCH_BLINKS,
                     COUNTOF(networkSearchBlinkPattern),
                     networkSearchBlinkPattern);
}

void emberAfPluginSilabsDeviceUiLedIdentifyBlink(void)
{
  halLedBlinkPattern(DEFAULT_NUM_IDENTIFY_BLINKS,
                     COUNTOF(networkIdentifyBlinkPattern),
                     networkIdentifyBlinkPattern);
}

void emberAfPluginSilabsDeviceUiLedProactiveRejoinBlink(void)
{
  halLedBlinkPattern(DEFAULT_NUM_PA_REJOIN_BLINKS,
                     COUNTOF(networkProactiveRejoinBlinkPattern),
                     networkProactiveRejoinBlinkPattern);
}

void emberAfPluginConnectionManagerStartNetworkSearchCallback(void)
{
  emberAfPluginSilabsDeviceUiLedNetworkSearchingBlink();
}

void emberAfPluginConnectionManagerLeaveNetworkCallback(void)
{
  emberAfPluginSilabsDeviceUiLedNetworkLostBlink();
}

void emberAfPluginConnectionManagerFinishedCallback(EmberStatus status)
{
  halLedBlinkLedOff(0);
}

//------------------------------------------------------------------------------
// Plugin private function implementations

// This function will cycle through all of the endpoints in the system and
// enable identify mode for each of them.
static void enableIdentify(void)
{
  uint8_t endpoint;
  uint8_t i;
  uint16_t identifyTimeS = IDENTIFY_ENABLE_LENGTH_S;

  for (i = 0; i < emberAfEndpointCount(); i++) {
    endpoint = emberAfEndpointFromIndex(i);
    if (emberAfContainsServer(endpoint, ZCL_IDENTIFY_CLUSTER_ID)) {
      emberAfWriteServerAttribute(endpoint,
                                  ZCL_IDENTIFY_CLUSTER_ID,
                                  ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                                  (uint8_t *) &identifyTimeS,
                                  ZCL_INT16U_ATTRIBUTE_TYPE);
    }
  }
}
