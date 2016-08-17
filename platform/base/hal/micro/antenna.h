/** @file hal/micro/antenna.h
 * @brief Antenna mode control functions.
 * See @ref micro for documentation.
 *
 * <!-- Copyright 2015 Silicon Laboratories, Inc.                        *80*-->
 */

/** @addtogroup micro
 * See also hal/micro/antenna.h for source code.
 *@{
 */

#ifndef __ANTENNA_H__
#define __ANTENNA_H__

#ifdef ANTENNA_DIVERSITY_DEFAULT_ENABLED
#define HAL_ANTENNA_MODE_DEFAULT HAL_ANTENNA_MODE_DIVERSITY
#else
#define HAL_ANTENNA_MODE_DEFAULT HAL_ANTENNA_MODE_ENABLE1
#endif

#ifndef DOXYGEN_SHOULD_SKIP_THIS
#ifndef __EMBERSTATUS_TYPE__
#define __EMBERSTATUS_TYPE__
  //This is necessary here because halSetAntennaMode returns an
  //EmberStatus and not adding this typedef to this file breaks a
  //whole lot of builds due to include mis-ordering
  typedef uint8_t EmberStatus;
#endif //__EMBERSTATUS_TYPE__
#endif // DOXYGEN_SHOULD_SKIP_THIS

typedef enum {
  HAL_ANTENNA_MODE_ENABLE1   = 0,
  HAL_ANTENNA_MODE_ENABLE2   = 1,
  HAL_ANTENNA_MODE_DIVERSITY = 2
} HalAntennaMode;

/** @brief Set antenna mode
 *
 * @param mode HAL_ANTENNA_MODE_ENABLE1 - enable antenna 1
 * HAL_ANTENNA_MODE_ENABLE2 - enable antenna 2
 * HAL_ANTENNA_MODE_DIVERSITY - toggle antenna if tx ack fails
 *
 * @return EMBER_SUCCESS if antenna mode is configured as desired
 * or EMBER_BAD_ARGUMENT if antenna mode is unsupported
 * been configured by the BOARD_HEADER.
 */
EmberStatus halSetAntennaMode(HalAntennaMode mode);


/** @brief Returns the current antenna mode.
 *
 * @return the current antenna mode.
 */
HalAntennaMode halGetAntennaMode(void);

/** @brief Toggle the enabled antenna
 *
 * @return EMBER_SUCCESS if antenna was toggled, EMBER_ERR_FATAL otherwise
 */
EmberStatus halToggleAntenna(void);

#endif //__ANTENNA_H__
/**@} // END micro group
 */

