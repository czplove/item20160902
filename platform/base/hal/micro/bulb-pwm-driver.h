// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#ifndef __BULB_PWM_DRIVER_H__
#define __BULB_PWM_DRIVER_H__

#include "hal/micro/micro.h"

#include PLATFORM_HEADER
#include "include/error.h"
#include "hal/micro/cortexm3/flash.h"

/** @brief Macro to signal that we wish to use the default frequency for the 
 *  PWM driver.
 */
#define HAL_BULB_PWM_DRIVER_USE_DEFAULT_FREQUENCY 0xffff

/** @brief Macro to signal that we wish to blink forever.  It is used for the 
 * count argument for the blink APIs below.
 */
#define HAL_BULB_PWM_DRIVER_BLINK_FOREVER 0xFF

/** @brief Return the ticks per PWM period.
 *
 * This function will examine the frequency configuration and determine the 
 * number of PWM ticks required to implement that frequency.  
 *
 * @return An ::uint16_t value that is the number of PWM ticks per period.
 */
uint16_t halBulbPwmDriverTicksPerPeriod(void);

/** @brief Return the ticks per microsecond.
 *
 * This function will return the number of PWM ticks per microsecond.  
 *
 * @return An ::uint16_t value that is the number of PWM ticks per microsecond.
 */
uint16_t halBulbPwmDriverTicksPerMicrosecond(void);

/** @brief Function to drive selected pwm.
 *
 * This function will set the compare registers for the specified PWM channel.
 * Note:  the channels are specified by the 
 *
 * @param value:  value for the PWM comparison register.
 *
 * @param pwm:  PWM channel to set (as defined by the board header file).
 *
 */
void halBulbPwmDriverSetPwmLevel(uint16_t value, uint8_t pwm);

/** @brief Function to turn on the LED output.  
 *
 * Function to turn the LED on full brightness as an indication to the 
 * user.  After the time, the LED will be reset to the appropriate values
 * as determined by the level, on/off, and color control cluster (if
 * appropriate).  
 *
 * @param time:  Number of seconds to turn the LED on.  0 means forever.
 */
void halBulbPwmDriverLedOn(uint8_t time);

/** @brief Function to turn off the LED output.  
 *
 * Function to turn the LED off as an indication to the user.  After the time, 
 * the LED will be reset to the appropriate values as determined by the level, 
 * on/off, and color control cluster (if appropriate).  
 *
 * @param time:  Number of seconds to turn the LED off.  0 means forever.
 */
void halBulbPwmDriverLedOff(uint8_t time);

/** @brief Blink the LED.
 *
 * Function to blink the LED as an indication to the user.  Note:  this will
 * blink the LED symmetrically.  If asymmetric blinking is required, please 
 * use the function ::emberAfPluginBulbPwmDriverLedBlinkPattern(...).
 *
 * @param count:  Number of times to blink.  HAL_BULB_PWM_DRIVER_BLINK_FOREVER 
 * means forever.  
 *
 * @param blinkTime:  Amount of time the bulb will be on or off during the
 * blink.
 */
void halBulbPwmDriverLedBlink(uint8_t count, uint16_t blinkTime);

/** @brief Blink a pattern on the LED.
 *
 * Function to blink a pattern on the LED.  User sets up a pattern of on/off
 * events.  
 * 
 * @param count:  Number of times to blink the pattern.  
 * HAL_BULB_PWM_DRIVER_BLINK_FOREVER means forever
 * 
 * @param length:  Length of the pattern.  20 is the maximum length.
 *
 * @param pattern[]:  Series of on/off times for the blink pattern.  
 *
 */
void halBulbPwmDriverLedBlinkPattern(uint8_t count, 
                                     uint8_t length, 
                                     uint16_t *pattern);

/** @brief Function to turn on the status LED output.  
 *
 * Function to turn the status LED on.  This is intended to be used to indicate
 * network status on the lighting reference design hardware.
 *
 * @param time:  Number of seconds to turn the LED on.  0 means forever.
 */
void halBulbPwmDriverStatusOn(uint8_t time);

/** @brief Function to turn off the status LED output.  
 *
 * Function to turn the status LED off.  This is intended to be used to
 * indicate network status on the lighting reference design hardware.
 *
 * @param time:  Number of seconds to turn the LED off.  0 means forever.
 */
void halBulbPwmDriverStatusOff(uint8_t time);

/** @brief Blink the status LED.
 *
 * Function to blink the status LED.  This is intended to be used to indicate
 * network status on the lighting reference design hardware
 *
 * @param count:  Number of times to blink.  HAL_BULB_PWM_DRIVER_BLINK_FOREVER 
 * means forever.  
 *
 * @param blinkTime:  Amount of time the bulb will be on or off during the
 * blink.
 */
void halBulbPwmDriverStatusBlink(uint8_t count, uint16_t blinkTime);

/** @brief Blink a pattern on the status LED.
 *
 * Function to blink a pattern on the status LED.  This is intended to be used
 * to indicate network status on the lighting reference design hardware.
 * 
 * @param count:  Number of times to blink the pattern.  
 * HAL_BULB_PWM_DRIVER_BLINK_FOREVER means forever
 * 
 * @param length:  Length of the pattern.  20 is the maximum length.
 *
 * @param pattern[]:  Series of on/off times for the blink pattern.  
 *
 */
void halBulbPwmDriverStatusBlinkPattern(uint8_t count, 
                                        uint8_t length, 
                                        uint16_t *pattern);

/** @brief Turn status LED on.
 *
 * Function to turn the status LED on.
 * 
 */
void halBulbPwmDriverStatusLedOn(void);

/** @brief Turn status LED off.
 *
 * Function to turn the status LED off.
 * 
 */
void halBulbPwmDriverStatusLedOff(void);

#endif
