// *****************************************************************************
// * power-meter.h
// *
// * API for reading power-meter data from a power-metering device
// *
// * Copyright 2015 Silicon Laboratories, Inc.                              *80*
// *****************************************************************************

// Some implementations of this function may make use of ::EmberEventControl
// events.  If so, the application framework will generally manage the events
// automatically.  Customers who do not use the framework must ensure the events
// are run, by calling either ::emberRunEvents or ::emberRunTask.  See the
// specific implementation for more information.

#ifndef __POWER_METER_H__
#define __POWER_METER_H__
//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------
typedef struct {
   int32_t dcVoltageOffset;    // calibrtion data for dc voltage offset
   int32_t dcCurrentOffset;    // calibrtion data for dc current offset
   int32_t acVoltageOffset;    // calibrtion data for ac voltage offset
   int32_t acCurrentOffset;    // calibrtion data for ac current offset
   int32_t temperatureOffset;  // calibrtion data for temperature offset
} HalPowerMeterCalibrationData;

/** @brief Over Current Status Change
 *
 * This function is called upon the status change of over current condition.
 *
 * @param status 0: changed from over current to normal; 1: over current
 * occured.  Ver.: always
 */
void emberAfPluginPowerMeterCs5463OverCurrentStatusChangeCallback(uint8_t status);

/** @brief Over Heat Status Change
 *
 * This function is called upon the status change of over heat condition.
 *
 * @param status 0: changed from over heat to normal; 1: over heat occured. 
 * Ver.: always
 */
void emberAfPluginPowerMeterCs5463OverHeatStatusChangeCallback(uint8_t status);

/** @brief Get Vrms in milliV
 *
 * This function will perform whatever hardware interaction is necessary to
 * read a Vrms value from the power meter and return it in units
 * of milli volts.
 *
 * @return The Vrms in milli Volt
 */
uint32_t halGetVrmsMilliV(void);

/** @brief Get Apparent Power in mW
 *
 * This function will perform whatever hardware interaction is necessary to
 * read a Apparent Power value from the power meter and return it in units
 * of milli Walt.
 *
 * @return The apparent power in milli Walt
 */
uint32_t halGetApparentPowerMilliW(void);

/** @brief Get Crms in milliA
 *
 * This function will perform whatever hardware interaction is necessary to
 * read a current value from the power meter and return it in units
 * of milli ampere.
 *
 * @return The Crms in milli Amp
 */
uint32_t halGetCrmsMilliA(void);

/** @brief Get Power Factor in the scale of 0-99
 *
 * This function will perform whatever hardware interaction is necessary to
 * read a power factor from the power meter and return it in 1/100s (0-99)
 * 
 *
 * @return Power facotr in 0-99
 */
int8_t halGetPowerFactor(void);

/** @brief Get Active Power in mW
 *
 * This function will perform whatever hardware interaction is necessary to
 * read a Active Power value from the power meter and return it in units
 * of milli Walt.
 *
 * @return The signed active power in milli Walt
 */
int32_t halGetActivePowerMilliW(void);

/** @brief Get Reactive Power in mW
 *
 * This function will perform whatever hardware interaction is necessary to
 * read a Reactive Power value from the power meter and return it in units
 * of milli Walt.
 *
 * @return The signed reactive power in milli Walt
 */
int32_t halGetReactivePowerMilliW(void);

/** @brief Get Temperature in Centi C
 *
 * This function will perform whatever hardware interaction is necessary to
 * read a temperature value from the power meter IC and return it in units
 * of CentiC.
 *
 * @return The Temperature in Centi C
 */
int16_t halGetPowerMeterTempCentiC(void);

/** @brief Get Status of power meter
 *
 * This function will get the status of Power meter so that it can fit into
 * the Simple Metering Server/status attribute (0x200)
 * Bit7 Reserved 
 * Bit6 Service Disconnect/Open
 * Bit5 Leak Detect
 * Bit4 Power Quality
 * Bit3 Power Failure
 * Bit2 Tamper Detect
 * Bit1 Low Battery
 * Bit0 CheckMeter
 * 
 * @return The status Byte
 */
uint8_t halGetPowerMeterStatus(void);

/** @brief Initializes the power meter hardware, along with any hardware
 * peripherals necessary to interface with the hardware.  The application
 * framework will generally initialize this plugin automatically.  Customers who
 * do not use the framework must ensure the plugin is intialized by calling
 * this function.
 */
void halPowerMeterInit(void);

/** @brief perform a calibration procedure needed by the power meter sensor
 * the input parameter temperature is a reference current temperature from the
 * caller, to calibrate the internal temperature sensor of the power meter.
 *
 * @param temperature current temperature in centi degree C
 *
 * @param pCalibrationData pointer the to be updated calibration data structure
 */
void halPowerMeterCalibrateAll(int16_t temperature,
                               HalPowerMeterCalibrationData * pCalibrationData);

/** @brief write a set of stored or predefined calibraiton data to the power meter
 * chip. 
 *
 * @param pointer to the calibration data structure
 *
 */
void halPowerMeterWriteCalibrateData(HalPowerMeterCalibrationData * pCalibrateData);

/** @brief Over Heat Callback
 *
 * This function is called upon the status change of over heat condition.
 *
 * @param status  OVER_HEAT_TO_NORMAL (0):changed from over heat to normal;
 *                NORMAL_TO_OVER_CURRENT (1):over heat occured.
 *
 */
void halPowerMeterCs5463OverHeatStatusChangeCallback(uint8_t status);

/** @brief Over Current Callback
 *
 * This function is called upon the status change of over current condition.
 * 
 * @param status OVER_CURRENT_TO_NORMAL (0):changed from over current to normal;
 *               NORMAL_TO_OVER_CURRENT (1):over current occured.
 *
 */
void halPowerMeterCs5463OverCurrentStatusChangeCallback(uint8_t status);
#endif //__POWER_METER_H__
