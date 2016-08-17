// *******************************************************************
// * led-temp-pwm.c
// *
// * Implements the color control server for color temperature bulbs.  Note: 
// * this is HA certifable and has passed HA certification for one customer
// * project.  
// *
// * Copyright 2015 by Silicon Laboratories. All rights reserved.           *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/util/attribute-storage.h"
#include "hal/micro/bulb-pwm-driver.h"

#include "app/framework/plugin-soc/led-temp-pwm/led-temp-pwm.h"

#ifdef EMBER_AF_PLUGIN_SCENES
  #include "app/framework/plugin/scenes/scenes.h"
#endif //EMBER_AF_PLUGIN_SCENES

#ifdef EMBER_AF_PLUGIN_ON_OFF
  #include "app/framework/plugin/on-off/on-off.h"
#endif //EMBER_AF_PLUGIN_ON_OFF

#ifdef EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER
  #include "app/framework/plugin/zll-level-control-server/zll-level-control-server.h"
#endif //EMBER_AF_PLUGIN_ZLL_LEVEL_CONTROL_SERVER

#ifdef EMBER_AF_PLUGIN_CONFIGURATION_SERVER
  #include "app/framework/plugin-soc/configuration-server/configuration-server.h"
#endif

#define MIN_ON_TIME_MICROSECONDS 0
#ifndef BULB_PWM_WHITE
  #define BULB_PWM_WHITE 1
#endif

#ifndef BULB_PWM_LOWTEMP
  #define BULB_PWM_LOWTEMP 2
#endif

#define OFF_TICKS 0
#define MAX_LEVEL 254

// ---------- Hardware values required for computing drive levels ----------
static uint16_t minColor, maxColor;
static uint16_t minPwmDrive, maxPwmDrive;
static bool blinking;

#define MIN_COLOR_DEFAULT 155
#define MAX_COLOR_DEFAULT 360

static uint16_t minDriveValue(void);
static uint16_t maxDriveValue(void);
static void updateDriveLevel(uint8_t endpoint);

static uint8_t currentEndpoint(void)
{
  // Note:  LED bulbs only support one endpoint
  assert(emberAfEndpointCount() == 1);

  return(emberAfEndpointFromIndex(0));
}

static void driveTempHiLo(uint16_t white, uint16_t lowtemp)
{
  halBulbPwmDriverSetPwmLevel(white, BULB_PWM_WHITE);
  halBulbPwmDriverSetPwmLevel(lowtemp, BULB_PWM_LOWTEMP);
}

void emberAfPluginLedTempPwmInitCallback(void)
{
  uint8_t endpoint = currentEndpoint();
  EmberAfStatus status;

  blinking = false;

  minPwmDrive = minDriveValue();
  maxPwmDrive = maxDriveValue();

  status = emberAfReadServerAttribute(endpoint,
                                      ZCL_COLOR_CONTROL_CLUSTER_ID,
                                      ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MIN_ATTRIBUTE_ID,
                                      (uint8_t *)&minColor,
                                      sizeof(minColor));

  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfCorePrintln("Color Temp:  no color temp physical min attribute.");
    minColor = MIN_COLOR_DEFAULT;
  }

  status = emberAfReadServerAttribute(endpoint,
                                      ZCL_COLOR_CONTROL_CLUSTER_ID,
                                      ZCL_COLOR_CONTROL_COLOR_TEMP_PHYSICAL_MAX_ATTRIBUTE_ID,
                                      (uint8_t *)&maxColor,
                                      sizeof(maxColor));

  if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfCorePrintln("Color Temp:  no color temp physical max attribute.");
    maxColor = MAX_COLOR_DEFAULT;
  }
  
  updateDriveLevel(endpoint);
}

static void computeRgbFromColorTemp(uint8_t endpoint)
{
  uint16_t currentTemp;
  uint8_t onOff, currentLevel;

  uint32_t high32, low32;
  uint16_t highDrive, lowDrive;

  if (blinking) {
    // we are in a mode where we are blinking an output pattern.  Don't blink
    // anything
    return;
  }

  // during framework init, this funciton sometimes is called before we set up
  // the values for max/min color temperautre.  
  if (maxColor == 0 || minColor == 0) {
    return;
  }

  emberAfReadServerAttribute(endpoint,
                             ZCL_COLOR_CONTROL_CLUSTER_ID,
                             ZCL_COLOR_CONTROL_COLOR_TEMPERATURE_ATTRIBUTE_ID,
                             (uint8_t *)&currentTemp,
                             sizeof(currentTemp));
  
  emberAfReadServerAttribute(endpoint,
                             ZCL_LEVEL_CONTROL_CLUSTER_ID,
                             ZCL_CURRENT_LEVEL_ATTRIBUTE_ID,
                             (uint8_t *)&currentLevel,
                             sizeof(currentLevel));

  emberAfReadServerAttribute(endpoint,
                             ZCL_ON_OFF_CLUSTER_ID,
                             ZCL_ON_OFF_ATTRIBUTE_ID,
                             (uint8_t *)&onOff,
                             sizeof(onOff));

  if (onOff == 0 || currentLevel == 0) {
    driveTempHiLo(OFF_TICKS, OFF_TICKS);
    
    return;
  }

  //bounds checking of the attribute temp. 
  if (currentTemp > maxColor) {
    currentTemp = maxColor;
  } else if (currentTemp < minColor) {
    currentTemp = minColor;
  }
     
  // Compute the low and high drive based on linear interpolation.
  // y=mx+b, where y = low temperature LED drive, 
  // m = (PWM Range) / (color range)
  // x = color temp - minimum color temperature
  // b = minimum PWM drive.  
  low32 = maxPwmDrive - minPwmDrive;
  low32 *= (currentTemp - minColor);
  low32 /= (maxColor - minColor);
  low32 += minPwmDrive;

  // y=mx+b, where y = high temperature LED drive
  // m = (PWM Range)/(color range)
  // x = maxColor - currentTemp
  // b = minPwmDrive.  
  high32 = maxPwmDrive - minPwmDrive;
  high32 *= (maxColor - currentTemp);
  high32 /= (maxColor - minColor);
  high32 += minPwmDrive;

  // Re-factor PWM drive values based on the current level.
  high32 *= currentLevel;
  high32 /= MAX_LEVEL;
  if (high32 < minPwmDrive && high32 != 0)
    high32 = minPwmDrive;

  low32 *= currentLevel;
  low32 /= MAX_LEVEL;
  if (low32 < minPwmDrive && low32 !=0)
    low32 = minPwmDrive;

  // convert to uint16_t and drive the PWMs.
  highDrive = (uint16_t) high32;
  lowDrive = (uint16_t) low32;
  
  driveTempHiLo(highDrive, lowDrive);

}

/** @brief Server Attribute Changedyes.
 *
 * Level Control cluster, Server Attribute Changed
 *
 * @param endpoint Endpoint that is being initialized  Ver.: always
 * @param attributeId Attribute that changed  Ver.: always
 */
void emberAfLevelControlClusterServerAttributeChangedCallback(uint8_t endpoint,
                                                              EmberAfAttributeId attributeId)
{
  computeRgbFromColorTemp(endpoint);
}

void emberAfOnOffClusterServerAttributeChangedCallback(uint8_t endpoint, 
                                                       EmberAfAttributeId attributeId)
{
  emberAfLevelControlClusterServerAttributeChangedCallback(endpoint, attributeId);
}

/** @brief Color Control Cluster Server Attribute Changed
 *
 * Server Attribute Changed
 *
 * @param endpoint Endpoint that is being initialized  Ver.: always
 * @param attributeId Attribute that changed  Ver.: always
 */
void emberAfColorControlClusterServerAttributeChangedCallback(uint8_t endpoint,
                                                              EmberAfAttributeId attributeId)
{
  emberAfLevelControlClusterServerAttributeChangedCallback(endpoint, attributeId);
}

static void updateDriveLevel(uint8_t endpoint)
{
  emberAfLevelControlClusterServerAttributeChangedCallback(endpoint, 0x0000);
}

// Part of the bulb driver is the minimum/maximum drive times as well as
// the frequency. 
static uint16_t minDriveValue(void)
{
  uint16_t minDriveValue;

#ifdef EMBER_AF_PLUGIN_CONFIGURATION_SERVER
  minDriveValue = emberAfPluginConfigurationServerReadMinOnTimeUs();
  if (minDriveValue == BULB_PWM_TIME_UNSET) {
    minDriveValue = MIN_ON_TIME_MICROSECONDS;
  }
#else
  minDriveValue = MIN_ON_TIME_MICROSECONDS;
#endif

  minDriveValue *= halBulbPwmDriverTicksPerMicrosecond();

  return minDriveValue;
}

static uint16_t maxDriveValue(void)
{
  uint16_t maxDriveValue;

#ifdef EMBER_AF_PLUGIN_CONFIGURATION_SERVER
  maxDriveValue = emberAfPluginConfigurationServerReadMaxOnTimeUs();
  if (maxDriveValue == BULB_PWM_TIME_UNSET) {
    maxDriveValue = halBulbPwmDriverTicksPerPeriod();
  } else {
    maxDriveValue *= halBulbPwmDriverTicksPerMicrosecond();
  }
#else
  maxDriveValue = halBulbPwmDriverTicksPerPeriod();
#endif
  
  // This bulb implemetnation has no 
  return maxDriveValue;
}

// **********************************************
// HAL callbacks.

/** @brief After the PWM driver has been initialized, we need to kick off 
 * the first value.  
 *
 * @appusage Should be implemented by an applicaiton layer configuration
 * plugin.
 *
 */
void halBulbPwmDriverInitCompleteCallback(void)
{
  updateDriveLevel(currentEndpoint());
}

/** @brief During blinking behavior, we rely on the bulb implementation code
 * to tell us how to turn the bulb on.
 *
 * @appusage Should be implemented by an applicaiton layer configuration
 * plugin.
 *
 */
void halBulbPwmDriverBlinkOnCallback(void)
{
  halBulbPwmDriverSetPwmLevel(halBulbPwmDriverTicksPerPeriod(), 
                              BULB_PWM_WHITE);
  halBulbPwmDriverSetPwmLevel(halBulbPwmDriverTicksPerPeriod(), 
                              BULB_PWM_LOWTEMP);
}

/** @brief During blinking behavior, we rely on the bulb implementation code
 * to tell us how to turn the bulb off.
 *
 * @appusage Should be implemented by an applicaiton layer configuration
 * plugin.
 *
 */
void halBulbPwmDriverBlinkOffCallback(void)
{
  halBulbPwmDriverSetPwmLevel(OFF_TICKS, BULB_PWM_WHITE);
  halBulbPwmDriverSetPwmLevel(OFF_TICKS, BULB_PWM_LOWTEMP);
}

/** @brief When we start blinking behavior, we need to tell the bulb
 * implementation code to hold off on any changes requried from the 
 * application.  I.e. don't change the dim level or color during a blink
 * event.
 *
 * @appusage Should be implemented by an applicaiton layer configuration
 * plugin.
 *
 */
void halBulbPwmDriverBlinkStartCallback(void)
{
  blinking = true;
}

/** @brief When we finish blinking behavior, we need the bulb implementation
 * code to resumem normal non-blinking behavoir.
 * 
 * @appusage Should be implemented by an applicaiton layer configuration
 * plugin.
 *
 */
void halBulbPwmDriverBlinkStopCallback(void)
{
  blinking = false;
  updateDriveLevel(currentEndpoint());
}

void emberAfPluginColorControlServerComputePwmFromTempCallback(uint8_t endpoint)
{
  computeRgbFromColorTemp(endpoint);
}
