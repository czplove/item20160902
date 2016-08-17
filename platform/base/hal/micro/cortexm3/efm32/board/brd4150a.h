/** @file hal/micro/cortexm3/efm32/board/brd4150a.h
 * BRD4150A-EFR32MG 2.4 GHz / 868 MHz 13dBm (SLWSTK6001A)
 * See @ref board for detailed documentation.
 *
 * <!-- Copyright 2016 Silicon Laboratories, Inc.                        *80*-->
 */

/** @addtogroup board
 *  @brief Functions and definitions specific to the breakout board.
 *
 *@{
 */

#ifndef __BOARD_H__
#define __BOARD_H__

#include "bspconfig.h"

// Define board features

// #define DISABLE_PTI

// #define DISABLE_DCDC

// Enable COM port to use retarget serial configuration
#define COM_RETARGET_SERIAL
// Enable HW flow control
#define COM_USART0_HW_FC

// TODO: Temporary workaround: in Connect we need to initialize the PA with
// the subGHz parameters. But, only in case where we are not using the board
// in the 2.4 GHz band.
#if defined(EMBER_STACK_CONNECT) && !defined(CONNECT_2_4_PHY)
#define halInternalConfigPa() do {                                 \
  RADIO_PAInit_t paInit = RADIO_PA_SUBGIG_INIT;                    \
  RADIO_PA_Init(&paInit);                                          \
} while(0)
#else //!EMBER_STACK_CONNECT
#define halInternalConfigPa() do {                                 \
  RADIO_PAInit_t paInit = RADIO_PA_2P4_INIT;                       \
  RADIO_PA_Init(&paInit);                                          \
} while(0)
#endif // !EMBER_STACK_CONNECT

// spi cs connection not the default
#define EXTERNAL_FLASH_CS_PORT      gpioPortA
#define EXTERNAL_FLASH_CS_PIN       4

// Include common definitions
#include "board/wstk-common.h"

#endif //__BOARD_H__
/** @} END Board Specific Functions */

/** @} END addtogroup */
