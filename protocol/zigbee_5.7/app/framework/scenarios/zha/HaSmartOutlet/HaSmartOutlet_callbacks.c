// Copyright 2015 Silicon Laboratories, Inc.                                *80*

#include "app/framework/include/af.h"
#include "app/framework/plugin/reporting/reporting.h"
#include "hal/micro/led-blink.h"
#include "hal/micro/led.h"
#include "hal/micro/cortexm3/power-meter-cs5463.h"
#include "app/framework/plugin-soc/connection-manager/connection-manager.h"
#include "app/framework/plugin-soc/illuminance-measurement-server/illuminance-measurement-server.h"
#include "app/framework/plugin-soc/temperature-measurement-server/temperature-measurement-server.h"
#include "app/framework/plugin-soc/relative-humidity-measurement-server/relative-humidity-measurement-server.h"
#include "app/framework/plugin-soc/electrical-measurement-server/electrical-measurement-server.h"
#ifdef EMBER_AF_PLUGIN_EZMODE_COMMISSIONING
#include "app/framework/plugin/ezmode-commissioning/ez-mode.h"
#endif //EMBER_AF_PLUGIN_EZMODE_COMMISSIONING

// Event control struct declarations
EmberEventControl buttonCountEventControl;

// defines for simulation tests
#ifdef EMBER_TEST
#define GPIO_PACLR_ADDR      1
#define GPIO_PASET_ADDR      1
#define GPIO_PAOUT_ADDR      1
#define GPIO_PACFGL_ADDR     1
#define GPIO_PBCFGL_ADDR     1 
#define P_RELAY_ON_PORT      1
#define P_RELAY_OFF_PORT     1
#define P_RELAY_ON           1
#define P_RELAY_OFF          1
#define POWER_LED            1
#define STATUS_LED           1
#endif


//-- GPIO access for nSEL signals
#define GPIO_PxCLR_BASE       (GPIO_PACLR_ADDR)
#define GPIO_PxSET_BASE       (GPIO_PASET_ADDR)
#define GPIO_PxOUT_BASE       (GPIO_PAOUT_ADDR)
#define GPIO_PxCFG_BASE       (GPIO_PACFGL_ADDR)
// Each port is offset from the previous port by the same amount
#define GPIO_Px_OFFSET        (GPIO_PBCFGL_ADDR - GPIO_PACFGL_ADDR)

#define LED_OVERHEAT_ON_TIME_MS         250
#define LED_OVERHEAT_OFF_TIME_MS        250
#define LED_OVERCURRENT_ON_TIME_MS      250
#define LED_OVERCURRENT_OFF_TIME_MS     750 
#define LED_POWERON_ON_TIME_MS         1000 
#define LED_POWEROFF_ON_TIME_MS         250
#define LED_POWERON_OFF_TIME_MS         250 
#define LED_POWEROFF_OFF_TIME_MS        250

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

#define DEFAULT_NUM_OVERHEAT_BLINKS \
      ((2*60)*(1000/(LED_OVERHEAT_ON_TIME_MS+LED_OVERHEAT_OFF_TIME_MS)))
#define DEFAULT_NUM_OVERCURRENT_BLINKS \
      ((2*60)*(1000/(LED_OVERCURRENT_ON_TIME_MS+LED_OVERCURRENT_OFF_TIME_MS)))
#define DEFAULT_NUM_POWERON_BLINKS       1
#define DEFAULT_NUM_POWEROFF_BLINKS      1

#define DEFAULT_NUM_JOIN_BLINKS      6
#define DEFAULT_NUM_LEAVE_BLINKS     3
#define DEFAULT_NUM_ON_NETWORK_BLINKS \
      ((3)*(1000/(LED_FOUND_BLINK_ON_TIME_MS+LED_FOUND_BLINK_OFF_TIME_MS)))

#define POWER_LED_OVERHEAT           1<<0
#define POWER_LED_OVERCURRENT        1<<1
#define POWER_LED_ON                 1<<2
#define POWER_LED_OFF                1<<3

#define POWER_BUTTON_MAX_MS          2000
#define POWER_BUTTON_MIN_MS           500
#define NETWORK_LEAVE_MIN_MS         3000
#define BUTTON_COUNT_TIMEOUT_MS      2000

#define RELAY_SWTICH_STATE_KEEP_TIME_MS 20 

#define DEFAULT_IDENTIFY_TIME_S        120
#define POWER_STATE_OFF                  0
#define POWER_STATE_ON                   1
#define POWER_STATE_TOGGLE               2

#define EZ_MODE_ENDPOINT                 1
#define EZ_MODE_NUMBER_OF_CLUSTERS       5

#define GPIO_RELAY_ON           P_RELAY_ON
#define GPIO_RELAY_OFF         P_RELAY_OFF

static bool powerException;
static uint8_t buttonCountShortPressed;
static bool networkSearching;
static uint16_t powerOnBlinkPattern[] = {
  LED_POWERON_ON_TIME_MS,LED_POWERON_OFF_TIME_MS}; 
static uint16_t powerOffBlinkPattern[] = {
  LED_POWEROFF_ON_TIME_MS,LED_POWEROFF_OFF_TIME_MS};
static uint16_t overHeatBlinkPattern[] = {
  LED_OVERHEAT_ON_TIME_MS,LED_OVERHEAT_OFF_TIME_MS};
static uint16_t overCurrentBlinkPattern[] = {
  LED_OVERCURRENT_ON_TIME_MS,LED_OVERCURRENT_OFF_TIME_MS};
static uint16_t onNetworkBlinkPattern[] = {
  LED_FOUND_BLINK_ON_TIME_MS,LED_FOUND_BLINK_OFF_TIME_MS}; 
static uint16_t networkLostBlinkPattern[] =
  { LED_LOST_ON_TIME_MS, LED_LOST_OFF_TIME_MS };
static uint16_t networkSearchBlinkPattern[] =
  { LED_BLINK_ON_TIME_MS, LED_SEARCH_BLINK_OFF_TIME_MS };
static uint16_t networkIdentifyBlinkPattern[] =
  { LED_IDENTIFY_ON_TIME_MS, LED_IDENTIFY_OFF1_TIME_MS,
    LED_IDENTIFY_ON_TIME_MS, LED_IDENTIFY_OFF2_TIME_MS };
#ifdef EMBER_AF_PLUGIN_EZMODE_COMMISSIONING
static const EmberAfClusterId ezModeClusterList[] = {
    ZCL_ON_OFF_CLUSTER_ID,
    ZCL_ILLUM_MEASUREMENT_CLUSTER_ID,
    ZCL_TEMP_MEASUREMENT_CLUSTER_ID,
    ZCL_RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_ID,
    ZCL_ELECTRICAL_MEASUREMENT_CLUSTER_ID,
    ZCL_SIMPLE_METERING_CLUSTER_ID,
  };
#endif

// Event function forward declarations
void buttonCountEventHandler(void);

#if ((!(defined (EMBER_TEST))) && (!(defined(CORTEXM3_EFR32))))
static void internalGpioClr(uint8_t gpio)
{
  *((volatile uint32_t *)(GPIO_PxCLR_BASE + (GPIO_Px_OFFSET * (gpio / 8))))\
                        = BIT(gpio & 7);
}

static void internalGpioSet(uint8_t gpio)
{
  *((volatile uint32_t *)(GPIO_PxSET_BASE + (GPIO_Px_OFFSET * (gpio / 8))))\
                        = BIT(gpio & 7);
}
#endif

static void relayOnOff(uint8_t status)
{
#ifndef EMBER_TEST
#if defined(CORTEXM3_EFR32)
  if (status) { 
    if (!powerException) {
      GPIO_PinOutClear(P_RELAY_ON_PORT, P_RELAY_ON_PIN);
      GPIO_PinOutSet(P_RELAY_OFF_PORT, P_RELAY_OFF_PIN);  
      halCommonDelayMilliseconds(RELAY_SWTICH_STATE_KEEP_TIME_MS);
      GPIO_PinOutClear(P_RELAY_ON_PORT, P_RELAY_ON_PIN);
      GPIO_PinOutClear(P_RELAY_OFF_PORT, P_RELAY_OFF_PIN);
    }
  } else {
    GPIO_PinOutClear(P_RELAY_OFF_PORT, P_RELAY_OFF_PIN);
    GPIO_PinOutSet(P_RELAY_ON_PORT, P_RELAY_ON_PIN);  
    halCommonDelayMilliseconds(RELAY_SWTICH_STATE_KEEP_TIME_MS);
    GPIO_PinOutClear(P_RELAY_ON_PORT, P_RELAY_ON_PIN);
    GPIO_PinOutClear(P_RELAY_OFF_PORT, P_RELAY_OFF_PIN);
  }
#else 
  if (status) { 
    if (!powerException) {
      internalGpioClr(GPIO_RELAY_ON);
      internalGpioSet(GPIO_RELAY_OFF);
      halCommonDelayMilliseconds(RELAY_SWTICH_STATE_KEEP_TIME_MS);
      internalGpioClr(GPIO_RELAY_ON);
      internalGpioClr(GPIO_RELAY_OFF);
    }
  } else {
    internalGpioClr(GPIO_RELAY_OFF);
    internalGpioSet(GPIO_RELAY_ON);
    halCommonDelayMilliseconds(RELAY_SWTICH_STATE_KEEP_TIME_MS);
    internalGpioClr(GPIO_RELAY_ON);
    internalGpioClr(GPIO_RELAY_OFF);
  }
#endif // CORTEXM3_EFR32
#endif // EMBER_TEST
  return;
}

static void powerOnOff(uint8_t status, bool updateOnOffAttribute)
{
  static uint8_t powerState;
  uint8_t endpointIndex, endpoint;

  if (status == POWER_STATE_TOGGLE) {
    powerState ^= 1;
  } else {
    powerState = status;
  }
  relayOnOff(powerState);
  if (!powerException) {
    if (powerState) {
      halMultiLedBlinkPattern(DEFAULT_NUM_POWERON_BLINKS,
                              2,
                              powerOnBlinkPattern,
                              POWER_LED);
    } else {
      halMultiLedBlinkPattern(DEFAULT_NUM_POWEROFF_BLINKS,
                              2,
                              powerOffBlinkPattern,
                              POWER_LED);
    }
  }

  if (updateOnOffAttribute) {
    // Cycle through all endpoints, check to see if the endpoint has a 
    // on off server, and if so update that endpoint
    for (endpointIndex = 0; 
         endpointIndex < emberAfEndpointCount();
         endpointIndex++) {
      endpoint = emberAfEndpointFromIndex(endpointIndex);
      if (emberAfContainsServer(endpoint, ZCL_ON_OFF_CLUSTER_ID)) {
        // write current on/off value
        emberAfWriteAttribute(endpoint,
                              ZCL_ON_OFF_CLUSTER_ID,
                              ZCL_ON_OFF_ATTRIBUTE_ID,
                              CLUSTER_MASK_SERVER,
                              (uint8_t *)&powerState,
                              ZCL_BOOLEAN_ATTRIBUTE_TYPE);
      }
    }
  }
}

//------------------------------------------------------------------------------
// enableIdentify
// This function will cycle through all of the endpoints in the system and
// enable identify mode for each of them.
static void enableIdentify(void)
{
  uint8_t endpoint;
  uint8_t i;
  uint16_t identifyTimeS = DEFAULT_IDENTIFY_TIME_S;
  
  for (i = 0; i < emberAfEndpointCount(); i++) {
    endpoint = emberAfEndpointFromIndex(i);
    if (emberAfContainsServer(endpoint, ZCL_IDENTIFY_CLUSTER_ID)) {
      emberAfWriteAttribute(endpoint,
                            ZCL_IDENTIFY_CLUSTER_ID,
                            ZCL_IDENTIFY_TIME_ATTRIBUTE_ID,
                            CLUSTER_MASK_SERVER,
                            (uint8_t *) &identifyTimeS,
                            ZCL_INT16U_ATTRIBUTE_TYPE);
    }
  }
}

void buttonCountEventHandler(void)
{
  emberEventControlSetInactive(buttonCountEventControl);
  if (emberAfNetworkState() != EMBER_NO_NETWORK) {
    switch (buttonCountShortPressed) {
    case 1:
      //Do nothing;
      break;
    case 2:
      //Identify
      enableIdentify();
      break;
    case 3:
      //Network status
      emberAfCorePrintln("Blinking user requested network status");
      halMultiLedBlinkPattern(DEFAULT_NUM_ON_NETWORK_BLINKS,
                              2,
                              onNetworkBlinkPattern,
                              STATUS_LED);
      break;
    case 4:
      //Do nothing
      break;
    case 5:
      //factory reset
      emberAfPluginConnectionManagerResetJoinAttempts();
      emberAfPluginConnectionManagerLeaveNetworkAndStartSearchForNewOne();
      powerOnOff(POWER_STATE_OFF, false);
      break;
    case 6:
#ifdef EMBER_AF_PLUGIN_EZMODE_COMMISSIONING
      //ez-mode commissioning
      emberAfEzmodeClientCommission(EZ_MODE_ENDPOINT,
          (EmberAfEzModeCommissioningDirection)EMBER_AF_EZMODE_COMMISSIONING_SERVER_TO_CLIENT,
          (const uint16_t*)&ezModeClusterList,
          EZ_MODE_NUMBER_OF_CLUSTERS);
#endif
      break;
    default:
      break;
    }
  } else {
    // If not a network, then regardless of button presses or length, we want to
    // make sure we are looking for a network.
    emberAfPluginConnectionManagerResetJoinAttempts();
    emberAfPluginConnectionManagerLeaveNetworkAndStartSearchForNewOne();
  }
    
  buttonCountShortPressed = 0;
}

// This callback will execute any time the reporting intervals are modified.
// In order to verify the smart outlet is polling the sensors
// frequently enough, it is necessary to call the respetive functions to change
// actual measurement rate any time the reporting intervals are changed.
EmberAfStatus emberAfPluginReportingConfiguredCallback(
  const EmberAfPluginReportingEntry *entry)
{
  emberAfCorePrintln("Reporting Callback cluster : 0x%4x", 
                        entry->clusterId);
  if ((entry->clusterId == ZCL_TEMP_MEASUREMENT_CLUSTER_ID)
      && (entry->attributeId == ZCL_TEMP_MEASURED_VALUE_ATTRIBUTE_ID)
      && (entry->direction == EMBER_ZCL_REPORTING_DIRECTION_REPORTED)
      && (entry->endpoint != EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID)) {
    //Max interval is set in seconds, which is the same unit of time the
    //emberAfPluginTemperatureMeasurementServerSetMeasurementInterval 
    //expects in this API.
    emberAfCorePrintln("Temperature reporting interval set: %d", 
                        entry->data.reported.maxInterval);
    emberAfPluginTemperatureMeasurementServerSetMeasurementInterval(
      entry->data.reported.maxInterval);
  } else if ((entry->clusterId == ZCL_RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_ID)
      && (entry->attributeId == 
               ZCL_RELATIVE_HUMIDITY_MEASURED_VALUE_ATTRIBUTE_ID)
      && (entry->direction == EMBER_ZCL_REPORTING_DIRECTION_REPORTED)
      && (entry->endpoint != EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID)) {
    //Max interval is set in seconds, which is the same unit of time the
    //emberAfPluginRelativeHumidityMeasurementServerSetMeasurementInterval
    //expects in this API.
    emberAfCorePrintln("Humidity reporting interval set: %d", 
                        entry->data.reported.maxInterval);
    emberAfPluginRelativeHumidityMeasurementServerSetMeasurementInterval(
      entry->data.reported.maxInterval);
  } else if ((entry->clusterId == ZCL_ILLUM_MEASUREMENT_CLUSTER_ID)
      && (entry->attributeId == ZCL_ILLUM_MEASURED_VALUE_ATTRIBUTE_ID)
      && (entry->direction == EMBER_ZCL_REPORTING_DIRECTION_REPORTED)
      && (entry->endpoint != EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID)) {
    //Max interval is set in seconds, which is the same unit of time the
    //emberAfPluginIlluminanceMeasurementServerSetMeasurementInterval expects
    //in this API.
    emberAfCorePrintln("Illuminance reporting interval set: %d", 
                        entry->data.reported.maxInterval);
    emberAfPluginIlluminanceMeasurementServerSetMeasurementInterval(
      entry->data.reported.maxInterval);
  } else if ((entry->clusterId == ZCL_ELECTRICAL_MEASUREMENT_CLUSTER_ID)
      && (entry->direction == EMBER_ZCL_REPORTING_DIRECTION_REPORTED)
      && (entry->endpoint != EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID)) {
    //Max interval is set in seconds, which is the same unit of time the
    //emberAfPluginElectricalMeasurementServerSetMeasurementInterval expects
    //in this API.
    emberAfCorePrintln("Electrical Measurement reporting interval set: %d", 
                        entry->data.reported.maxInterval);
    emberAfPluginElectricalMeasurementServerSetMeasurementInterval(
      entry->data.reported.maxInterval);
  }
  return EMBER_ZCL_STATUS_SUCCESS;
}

/** @brief Server Init
 *
 * On/off cluster, Server Init
 *
 * @param endpoint Endpoint that is being initialized  Ver.: always
 */
void emberAfOnOffClusterServerInitCallback(uint8_t endpoint)
{
  // At startup, trigger a read of the attribute and possibly a toggle of the
  // LED to make sure they are always in sync.
  emberAfOnOffClusterServerAttributeChangedCallback(endpoint,
                                                    ZCL_ON_OFF_ATTRIBUTE_ID);
}

/** @brief Identify Start Feedback
 *
 * This function is called by the Identify plugin when identification begins.
 * It informs the Identify Feedback plugin that it should begin providing its
 * implemented feedback functionality (e.g. LED blinking, buzzer sounding,
 * etc.) until the Identify plugin tells it to stop. The identify time is
 * purely a matter of informational convenience; this plugin does not need to
 * know how long it will identify (the Identify plugin will perform the
 * necessary timekeeping.)
 *
 * @param endpoint The identifying endpoint Ver.: always
 * @param identifyTime The identify time Ver.: always
 */
void emberAfPluginIdentifyStartFeedbackCallback(uint8_t endpoint,
                                                uint16_t identifyTime)
{

  emberAfCorePrintln("Beginning identify blink pattern");
  halMultiLedBlinkPattern(DEFAULT_NUM_IDENTIFY_BLINKS,
                         4,
                         networkIdentifyBlinkPattern,
                         STATUS_LED);
}
/** @brief Identify Stop Feedback
 *
 * This function is called by the Identify plugin when identification is
 * finished. It tells the Identify Feedback plugin to stop providing its
 * implemented feedback functionality.
 *
 * @param endpoint The identifying endpoint Ver.: always
 */
void emberAfPluginIdentifyStopFeedbackCallback(uint8_t endpoint)
{
  emberAfCorePrintln("Identify has finished");
  halMultiLedBlinkLedOff(0, STATUS_LED);
}

/** @brief Connection Manager callback when finished forming or joining process
 *
 * This callback is fired when the Connection Manager plugin is finished with
 * the forming or joining process. The result of the operation will be returned
 * in the status parameter.
 *
 * @param status   Ver.: always
 */
void emberAfPluginConnectionManagerFinishedCallback(EmberStatus status)
{
  if (status == EMBER_NOT_JOINED) {
    networkSearching = false;
    halMultiLedBlinkLedOff(0, STATUS_LED);
  } else if (status == EMBER_SUCCESS) {
    networkSearching = false;
  }
}

/** @brief Connection Manager callbacks when it starts searching a network.
 *
 * This function is called by the Connection Manager Plugin when it starts 
 * to search a new network.  It is normally used to trigger a UI event to
 * notify the user that the device is currently searching for a network.
 */
void emberAfPluginConnectionManagerStartNetworkSearchCallback(void)
{
  if (!networkSearching) {
    networkSearching = true;
    halMultiLedBlinkPattern(DEFAULT_NUM_SEARCH_BLINKS,
                            2,
                            networkSearchBlinkPattern,
                            STATUS_LED);
  }
}

/** @brief Connection Manager callback when it leaves the network.
 *
 * This function is called by the Connection Manager Plugin when the device
 * leaves the network.  It is normally used to trigger a UI event to notify
 * the user of a network leave.
 */
void emberAfPluginConnectionManagerLeaveNetworkCallback(void)
{
  halMultiLedBlinkPattern(DEFAULT_NUM_LEAVE_BLINKS,
                          2,
                          networkLostBlinkPattern,
                          STATUS_LED);
}

/** @brief Stack Status
 *
 * This function is called by the application framework from the stack status
 * handler.  This callbacks provides applications an opportunity to be
 * notified of changes to the stack status and take appropriate action.  The
 * application should return true if the status has been handled and should
 * not be handled by the application framework.
 *
 * @param status   Ver.: always
 */
bool emberAfStackStatusCallback(EmberStatus status)
{
  uint8_t endpointCount, endpoint, currentValue;

  // If we go up or down, let the user know, although the down case shouldn't
  // happen.
  if (status == EMBER_NETWORK_UP) {
#ifdef EMBER_AF_PLUGIN_EZMODE_COMMISSIONING
    //Send out a broadcast pjoin, per HA spec, If the node is a ZigBee Router
    //and joins a network, it shall broadcast a PermitJoin time of at least
    //EZModeTime.
    emberAfPermitJoin(EMBER_AF_PLUGIN_EZMODE_COMMISSIONING_IDENTIFY_TIMEOUT,
                   true);
#endif
    halMultiLedBlinkPattern(DEFAULT_NUM_JOIN_BLINKS,
                            2,
                            onNetworkBlinkPattern,
                            STATUS_LED);
    endpointCount = emberAfEndpointCount(); 
    if (endpointCount == 1) {
      endpoint = emberAfEndpointFromIndex(0);
      if (emberAfContainsServer(endpoint, ZCL_ON_OFF_CLUSTER_ID)) {
        // read current on/off value
        status = emberAfReadAttribute(endpoint,
                                      ZCL_ON_OFF_CLUSTER_ID,
                                      ZCL_ON_OFF_ATTRIBUTE_ID,
                                      CLUSTER_MASK_SERVER,
                                      (uint8_t *)&currentValue,
                                      sizeof(currentValue),
                                      NULL); // data type
        if (status != EMBER_ZCL_STATUS_SUCCESS) {
          emberAfCorePrintln("ERR: reading on/off %x", status);
          return status;
        } else {
          powerOnOff(currentValue, false);
        }
      }
    } else if (endpointCount == 0) {
      emberAfCorePrintln("no endpoint found!");
    } else {
      emberAfCorePrintln("multiple endpoints not supported!");
    }
    networkSearching = false;
  } else if (status == EMBER_NETWORK_DOWN
             && emberAfNetworkState() == EMBER_NO_NETWORK) {
    halMultiLedBlinkPattern(DEFAULT_NUM_LEAVE_BLINKS,
                            2,
                            networkLostBlinkPattern,
                            STATUS_LED);
    networkSearching = false;
  } else if (status == EMBER_NETWORK_DOWN
             && emberAfNetworkState() == EMBER_JOINED_NETWORK_NO_PARENT) {
    halMultiLedBlinkLedOff(0, STATUS_LED);
    networkSearching = false;
  }
  return false;
}

/** @brief On/off Cluster Server Attribute Changed
 *
 * Server Attribute Changed
 *
 * @param endpoint Endpoint that is being initialized  Ver.: always
 * @param attributeId Attribute that changed  Ver.: always
 */
void emberAfOnOffClusterServerAttributeChangedCallback(uint8_t endpoint,
                                                EmberAfAttributeId attributeId)
{
  uint8_t onOff;

  // When the on/off attribute changes, set the LED appropriately.  If an error
  // occurs, ignore it because there's really nothing we can do.
  if (attributeId == ZCL_ON_OFF_ATTRIBUTE_ID) {
    if (emberAfReadServerAttribute(endpoint,
                                   ZCL_ON_OFF_CLUSTER_ID,
                                   ZCL_ON_OFF_ATTRIBUTE_ID,
                                   (uint8_t *)&onOff,
                                   sizeof(onOff))
        == EMBER_ZCL_STATUS_SUCCESS) {
      if ((powerException) && (onOff != POWER_STATE_OFF)) {
        onOff = POWER_STATE_OFF;
        emberAfWriteAttribute(endpoint,
                              ZCL_ON_OFF_CLUSTER_ID,
                              ZCL_ON_OFF_ATTRIBUTE_ID,
                              CLUSTER_MASK_SERVER,
                              (uint8_t *)&onOff,
                              ZCL_BOOLEAN_ATTRIBUTE_TYPE);
      } else {
        powerOnOff(onOff, false);
      }
    }
  }  
}

/** @brief Main Init
 *
 * This function is called from the application's main function. It gives the
 * application a chance to do any initialization required at system startup.
 * Any code that you would normally put into the top of the application's 
 * main() routine should be put into this function. This is called before the
 * clusters, plugins, and the network are initialized so some functionality is 
 * not yet available.
        Note: No callback in the Application Framework is
 * associated with resource cleanup. If you are implementing your application
 * on a Unix host where resource cleanup is a consideration, we expect that you
 * will use the standard Posix system calls, including the use of atexit() and
 * handlers for signals such as SIGTERM, SIGINT, SIGCHLD, SIGPIPE and so on. If
 * you use the signal() function to register your signal handler, please mind
 * the returned value which may be an Application Framework function. If the
 * return value is non-null, please make sure that you call the returned
 * function from your handler to avoid negating the resource cleanup of the
 * Application Framework itself.
 *
 */
void emberAfMainInitCallback(void)
{
#if (!(defined (EMBER_TEST)) && (defined(CORTEXM3_EFR32)))
  //config gpio ports for replay control  
  GPIO_PinModeSet(P_RELAY_ON_PORT, P_RELAY_ON_PIN, gpioModePushPull, 0);
  GPIO_PinModeSet(P_RELAY_OFF_PORT, P_RELAY_OFF_PIN, gpioModePushPull, 0);
#endif
  halMultiLedBlinkSetActivityLeds(POWER_LED);
  halMultiLedBlinkSetActivityLeds(STATUS_LED);
  buttonCountShortPressed = 0;
  networkSearching = false;
  halMultiLedBlinkPattern(DEFAULT_NUM_POWEROFF_BLINKS,
                          2,
                          powerOffBlinkPattern,
                          POWER_LED);
}

/** @brief Button0 Pressed Long
 *
 * This function returns the number of times a button was short pressed.
 *
 * @param timePressedMs Amount of time button 0 was pressed.  Ver.: always
 * @param pressedAtReset Was the button pressed at startup.  Ver.: always
 */
void emberAfPluginButtonInterfaceButton0PressedLongCallback(
                                                      uint16_t timePressedMs,
                                                      bool pressedAtReset)
{
  if (( timePressedMs < POWER_BUTTON_MAX_MS ) && ( !powerException )) {
    powerOnOff(POWER_STATE_TOGGLE, true);
    emberAfCorePrintln("Toggle Power!\r\n");
  } else if ( timePressedMs > NETWORK_LEAVE_MIN_MS ) { //leave network;
    emberAfCorePrintln("Leave and rescan!\r\n");
    emberAfPluginConnectionManagerResetJoinAttempts();
    emberAfPluginConnectionManagerLeaveNetworkAndStartSearchForNewOne();
  } 
}

/** @brief Button0 Pressed Short
 *
 * This function returns the number of times a button was short pressed.
 *
 * @param timePressedMs Time (in ms) button 0 was pressed  Ver.: always
 */
void emberAfPluginButtonInterfaceButton0PressedShortCallback(
                                                      uint16_t timePressedMs)
{
  buttonCountShortPressed++;
  emberEventControlSetDelayMS( buttonCountEventControl, 
                               BUTTON_COUNT_TIMEOUT_MS );
}

/** @brief Over Current Callback
 *
 * This function is called upon the status change of over current condition.
 * 
 * @param status OVER_CURRENT_TO_NORMAL: changed from over current to normal; 
 *               NORMAL_TO_OVER_CURRENT: over current occured.
 *
 */
void halPowerMeterCs5463OverCurrentStatusChangeCallback(
         uint8_t status)
{
  if (status == CS5463_NORMAL_TO_OVER_CURRENT) { // over current happened
    emberAfCorePrintln("Over Current!!\r\n");
    powerException = true;
    powerOnOff(POWER_STATE_OFF, true);
    halMultiLedBlinkPattern(DEFAULT_NUM_OVERCURRENT_BLINKS,
                            2,
                            overCurrentBlinkPattern,
                            POWER_LED);
  } else { // over current gone
    powerException = false;
  }
}

/** @brief Over Heat Callback
 *
 * This function is called upon the status change of over heat condition.
 *
 * @param status  OVER_HEAT_TO_NORMAL: changed from over heat to normal;
 *                NORMAL_TO_OVER_CURRENT: over heat occured.
 *
 */
void halPowerMeterCs5463OverHeatStatusChangeCallback(
         uint8_t status)
{
  if (status == CS5463_NORMAL_TO_OVER_HEAT) { // over heat happened
    emberAfCorePrintln("Over Heat!!\r\n");
    powerException = true;
    powerOnOff(POWER_STATE_OFF, true);
    halMultiLedBlinkPattern(DEFAULT_NUM_OVERHEAT_BLINKS,
                            2, 
                            overHeatBlinkPattern, 
                            POWER_LED);
  } else { // over heat gone
    powerException = false;
  }
}
