// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#include "app/framework/include/af.h"
#include "app/framework/plugin/reporting/reporting.h"
#include "app/framework/plugin-soc/illuminance-measurement-server/illuminance-measurement-server.h"
#include "app/framework/plugin-soc/relative-humidity-measurement-server/relative-humidity-measurement-server.h"
#include "app/framework/plugin-soc/temperature-measurement-server/temperature-measurement-server.h"
#include "app/framework/plugin-soc/connection-manager/connection-manager.h"
#include "app/framework/plugin-soc/silabs-device-ui/silabs-device-ui.h"
#include "hal/micro/led-blink.h"

#if defined(CORTEXM3_EFR32)
#include "bsp.h"
#include "bspconfig.h"
#endif

#define BLINK_OCCUPANCY_NOTIFICATION

// This variable will track whether the user has requested an override to the
// default sleep behavior
static bool okToSleep = true;

// This callback is generated when the occupancy measurement server receives
// a new occupancy status.  It will cause the occupancy detected pattern to
// blink when occupancy is detected.
void emberAfPluginOccupancySensorServerOccupancyStateChangedCallback(
  HalOccupancyState occupancyState)
{
#ifdef BLINK_OCCUPANCY_NOTIFICATION
  uint16_t occupancyDetectedBlinkPattern[] = {5, 100};
  if (occupancyState == HAL_OCCUPANCY_STATE_OCCUPIED) {
    halLedBlinkPattern(3, 2, occupancyDetectedBlinkPattern);
  }
#endif
}

// This callback will execute any time the reporting intervals are modified.
// In order to verify the occupancy sensor is polling the environment sensors
// frequently enough for the report intervals to be effective, it is necessary
// to call the SetMeasurementInterval function for each sensor any time the
// reporting intervals are changed.
EmberAfStatus emberAfPluginReportingConfiguredCallback(
  const EmberAfPluginReportingEntry *entry)
{
  if (entry->direction != EMBER_ZCL_REPORTING_DIRECTION_REPORTED) {
    return EMBER_ZCL_STATUS_SUCCESS;
  }

  if ((entry->clusterId == ZCL_TEMP_MEASUREMENT_CLUSTER_ID)
      && (entry->attributeId == ZCL_TEMP_MEASURED_VALUE_ATTRIBUTE_ID)) {

    // If setMeasurementInterval is called with a value of 0, the hardware will
    // revert to polling the hardware at the maximum rate, specified by the HAL
    // plugin.
    if (entry->endpoint == EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID) {
      emberAfCorePrintln("Temperature reporting disabled");
      emberAfPluginTemperatureMeasurementServerSetMeasurementInterval(0);
    } else {

      //Max interval is set in seconds, which is the same unit of time the
      //emberAfPluginTemperatureMeasurementServerSetMeasurementInterval expects
      // in this API.
      emberAfCorePrintln("Temperature reporting interval set: %d seconds",
                          entry->data.reported.maxInterval);
      emberAfPluginTemperatureMeasurementServerSetMeasurementInterval(
        entry->data.reported.maxInterval);
    }
  } else if ((entry->clusterId == ZCL_RELATIVE_HUMIDITY_MEASUREMENT_CLUSTER_ID)
             && (entry->attributeId
                 == ZCL_RELATIVE_HUMIDITY_MEASURED_VALUE_ATTRIBUTE_ID)) {

    // If setMeasurementInterval is called with a value of 0, the hardware will
    // revert to polling the hardware at the maximum rate, specified by the HAL
    // plugin.
    if (entry->endpoint == EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID) {
      emberAfCorePrintln("Relative Humidity reporting disabled");
      emberAfPluginRelativeHumidityMeasurementServerSetMeasurementInterval(0);
    } else {

      //Max interval is set in seconds, which is the same unit of time the
      //emberAfPluginRelativeHumidityMeasurementServerSetMeasurementInterval
      //expects in this API.
      emberAfCorePrintln("Humidity reporting interval set: %d seconds",
                          entry->data.reported.maxInterval);
      emberAfPluginRelativeHumidityMeasurementServerSetMeasurementInterval(
        entry->data.reported.maxInterval);
    }
  } else if ((entry->clusterId == ZCL_ILLUM_MEASUREMENT_CLUSTER_ID)
             && (entry->attributeId == ZCL_ILLUM_MEASURED_VALUE_ATTRIBUTE_ID)) {

    // If setMeasurementInterval is called with a value of 0, the hardware will
    // revert to polling the hardware at the maximum rate, specified by the HAL
    // plugin.
    if (entry->endpoint == EMBER_AF_PLUGIN_REPORTING_UNUSED_ENDPOINT_ID) {
      emberAfCorePrintln("Illuminance reporting disabled");
      emberAfPluginIlluminanceMeasurementServerSetMeasurementInterval(0);
    } else {

      //Max interval is set in seconds, which is the same unit of time the
      //emberAfPluginIlluminanceMeasurementServerSetMeasurementInterval expects 
      //in this API.
      emberAfCorePrintln("Illuminance reporting interval set: %d seconds",
                          entry->data.reported.maxInterval);
      emberAfPluginIlluminanceMeasurementServerSetMeasurementInterval(
        entry->data.reported.maxInterval);
    }
  }

  return EMBER_ZCL_STATUS_SUCCESS;
}

void emberAfPluginButtonInterfaceButton0PressedLongCallback(uint16_t timePressedMs,
                                                            bool pressedAtReset)
{
  if (okToSleep) {
    emberAfCorePrintln("Disabling sleepy behavior");
    okToSleep = false;
  } else {
    emberAfCorePrintln("Enabling sleepy behavior");
    okToSleep = true;
  }
}

bool emberAfPluginIdleSleepOkToSleepCallback(uint32_t durationMs)
{
  return okToSleep;
}

void emberAfMainInitCallback(void)
{
  EmberAfClusterId ezModeClusterList[] = {
    ZCL_OCCUPANCY_SENSING_CLUSTER_ID,
  };

  emberAfPluginSilabsDeviceUiSetEzModeClusters(ezModeClusterList, 
                                               COUNTOF(ezModeClusterList));

#if defined(CORTEXM3_EFR32)
  // Set up the LED configuration to have the LED turned off on boot
  halInternalInitLed();

  // Currently, the BSP init functions will only set the LED to be a push/pull
  // output.  As such, the pin needs to be manually configured if it is
  // intended to be used in any mode other than push/pull
  GPIO_PinModeSet(BSP_GPIO_LED0_PORT, BSP_GPIO_LED0_PIN, BSP_GPIO_LED0_MODE, 1);

  // Currently, the button init functions do not allow for setting the pin mode
  // to internal pull up/down.  As such, the pin needs to be manually
  // configured if it is to be used as intended with the reference design hw.
  GPIO_PinModeSet(BSP_GPIO_PB0_PORT, BSP_GPIO_PB0_PIN, gpioModeInputPull, 1);
#endif

  halLedBlinkLedOff(0);
}


