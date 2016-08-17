/** @file hal/micro/cortexm3/em35x/board/ist-a51.h
 * See @ref board for detailed documentation.
 *
 * <!-- Copyright 2015 Silicon Laboratories, Inc.                        *80*-->
 */

/** @addtogroup board
 *  @brief Functions and definitions specific to the smart outlet reference 
 *  design
 *
 * * @note The file ist-a51.h is intended to be copied, renamed, and
 * customized for customer-specific hardware.
 *
 * The file ist-a51.h is the default BOARD_HEADER file used with the smart
 * outlet reference design.
 *
 * The EM35x on a ist-a51 has the following example GPIO configuration.
 * This board file and the default HAL setup reflects this configuration.
 * - PA0 - nPWR_SEN_INT (interrupt pin from the power sensor)
 * - PA1 - SC2I2CSDA
 * - PA2 - SC2I2CSCL
 * - PA3 - NC
 * - PA4 - PTI_EN
 * - PA5 - PTI_DATA
 * - PA6 - LED (Network Status)
 * - PA7 - LED (Power Status)
 * - PB0 - PWR_ZCD (zero cross detection for power inlet)
 * - PB1 - SC0_MOSI
 * - PB2 - SC0_MISO
 * - PB3 - SC0_SCLK
 * - PB4 - SC0_nSSEL
 * - PB5 - nPWR_SEN_REESET (output to reset the power sensor)
 * - PB6 - Button
 * - PB7 - NC
 * - PC0 - JTAG (JRST)
 * - PC1 - NC
 * - PC2 - JTAG (JTDO) / SWO
 * - PC3 - JTAG (JTDI)
 * - PC4 - JTAG (JTMS)
 * - PC5 - TX Active
 * - PC6 - RELAY_ON (output to control the on pin of the relay)
 * - PC7 - RELAY_OFF (output to control the off pin of the relay)
 *
 *@{
 */

#ifndef __BOARD_H__
#define __BOARD_H__

/** @name LED Definitions
 *
 * The following are used to aid in the abstraction with the LED
 * connections.  The microcontroller-specific sources use these
 * definitions so they are able to work across a variety of boards
 * which could have different connections.  The names and ports/pins
 * used below are intended to match with a schematic of the system to
 * provide the abstraction.
 *
 * The ::HalBoardLedPins enum values should always be used when manipulating the
 * state of LEDs, as they directly refer to the GPIOs to which the LEDs are
 * connected.
 *
 * \b Note: LED 0 is the network status LED
 *
 * \b Note: LED 1 is the power status LED
 *
 */
//@{

/**
 * @brief Assign each GPIO with an LED connected to a convenient name.
 * ::BOARD_ACTIVITY_LED and ::BOARD_HEARTBEAT_LED provide a further layer of
 * abstraction on top of the 3 LEDs for verbose coding.
 */
typedef enum HalBoardLedPins {
  BOARDLED0 = PORTA_PIN(6),
  BOARDLED1 = PORTA_PIN(7),
  STATUS_LED = BOARDLED0,
  POWER_LED = BOARDLED1
} HalBoardLedPins_t;

/**
 * @brief Inform the led-blink plugin that this board only has two LED
 */
#define MAX_LED_NUMBER        2

/**
 * @brief Inform the led-blink plugin that LEDs are active high
 */
#define LED_ACTIVE_HIGH

/** @} END OF LED DEFINITIONS  */

/** @name Button Definitions
 *
 * The following are used to aid in the abstraction with the Button
 * connections.  The microcontroller-specific sources use these
 * definitions so they are able to work across a variety of boards
 * which could have different connections.  The names and ports/pins
 * used below are intended to match with a schematic of the system to
 * provide the abstraction.
 *
 * The BUTTONn macros should always be used with manipulating the buttons
 * as they directly refer to the GPIOs to which the buttons are connected.
 *
 * @note The GPIO number must match the IRQ letter
 */
//@{
/**
 * @brief The actual GPIO BUTTON0 is connected to.  This define should
 * be used whenever referencing BUTTON0.
 */
#define BUTTON0             PORTB_PIN(6)
/**
 * @brief The GPIO input register for BUTTON0.
 */
#define BUTTON0_IN          GPIO_PBIN
/**
 * @brief Point the proper IRQ at the desired pin for BUTTON0.
 * @note IRQB is fixed and as such does not need any selection operation.
 */
#define BUTTON0_SEL()       do { } while(0)
/**
 * @brief The interrupt service routine for BUTTON0.
 */
#define BUTTON0_ISR         halIrqBIsr
/**
 * @brief The interrupt configuration register for BUTTON0.
 */
#define BUTTON0_INTCFG      GPIO_INTCFGB
/**
 * @brief The interrupt enable bit for BUTTON0.
 */
#define BUTTON0_INT_EN_BIT  INT_IRQB
/**
 * @brief The interrupt flag bit for BUTTON0.
 */
#define BUTTON0_FLAG_BIT    INT_IRQBFLAG
/**
 * @brief The missed interrupt bit for BUTTON0.
 */
#define BUTTON0_MISS_BIT    INT_MISSIRQB
//@} //END OF BUTTON DEFINITIONS

/** @name I2C Definitions
 *
 * The following are used to aid in the abstraction with the I2C
 * peripheral. The microcontroller-specific sources use these
 * definitions so they are able to work across a variety of boards
 * which could have used different I2C ports. The names and ports/pins
 * used below are intended to match with a schematic of the system to
 * provide the abstraction.
 */
//@{
/**
 * @brief The I2C port to be used 
 */
#define I2C_SC_PORT   2
/**
 * @brief The GPIO port for I2C SDA 
 */
#define I2C_SDA_PORT  A
/**
 * @brief The GPIO pin for I2C SDA
 */
#define I2C_SDA_PIN   1
/**
 * @brief The GPIO port for I2C SCL
 */
#define I2C_SCL_PORT  A
/**
 * @brief The GPIO pin for I2C SCL
 */
#define I2C_SCL_PIN   2
//@} //END OF I2C DEFINITIONS

/** @name SPI Definitions
 *
 * The following are used to aid in the abstraction with the SPI
 * peripheral. The microcontroller-specific sources use these
 * definitions so they are able to work across a variety of boards
 * which could have used different SPI ports. The names and ports/pins
 * used below are intended to match with a schematic of the system to
 * provide the abstraction.
 */
//@{
/**
 * @brief Specifies the SPI port(s) to be supported
 */
#define ENABLE_SPI_SC 0
/**
 * @brief abstract the SPI port 0 MOSI pin
 */
#define SC0_MOSI        PORTB_PIN(1)
/**
 * @brief GPIO port for SPI 0 MOSI 
 */
#define SC0_MOSI_PORT   B
/**
 * @brief GPIO pin for SPI 0 MOSI 
 */
#define SC0_MOSI_BIT    1
/**
 * @brief abstract the SPI port 0 MISO pin
 */
#define SC0_MISO        PORTB_PIN(2)
/**
 * @brief GPIO port fot SPI 0 MISO 
 */
#define SC0_MISO_PORT   B
/**
 * @brief GPIO pin for SPI 0 MISO
 */
#define SC0_MISO_BIT    2
/**
 * @brief abstract the SPI port 0 SCLK pin
 */
#define SC0_SCLK        PORTB_PIN(3)
/**
 * @brief GPIO port fot SPI 0 SCLK
 */
#define SC0_SCLK_PORT   B
/**
 * @brief GPIO pin for SPI 0 SCLK
 */
#define SC0_SCLK_BIT    3
/**
 * @brief abstract the SPI port 0 NSEL pin
 */
#define SC0_NSEL        PORTB_PIN(4)
/**
 * @brief GPIO port fot SPI 0 NSEL
 */
#define SC0_NSEL_PORT   B
/**
 * @brief GPIO pin for SPI 0 NSEL
 */
#define SC0_NSEL_BIT    4
//@} //END OF SPI DEFINITIONS

/** @name smart outlet configurations
 *
 * The following are used to abstract gpio contol of power replay and 
 * reset pin of the power sensor, it is specific for smart outlet project
 */
//@{
/**
 * @brief GPIO pin for relay on control
 */
#define P_RELAY_ON         PORTC_PIN(6)
/**
 * @brief GPIO pin for relay off control
 */
#define P_RELAY_OFF        PORTC_PIN(7)
/**
 * @brief GPIO pin to reset the power sensor
 */
#define PWR_SEN_RESET   PORTB_PIN(5)
//@} //END OF SMART OUTLET CONFIGURATION

/** @name Packet Trace
 *
 * When ::PACKET_TRACE is defined, ::GPIO_PACFGH will automatically be setup by
 * halInit() to enable Packet Trace support on PA4 and PA5,
 * in addition to the configuration specified below.
 *
 * @note This define will override any settings for PA4 and PA5.
 */
//@{
/**
 * @brief This define does not equate to anything.  It is used as a
 * trigger to enable Packet Trace support on the breakout board (dev0680).
 */
#define PACKET_TRACE  // We do have PACKET_TRACE support
//@} //END OF PACKET TRACE DEFINITIONS

/** @name GPIO Configuration Definitions
 *
 * The following are used to specify the GPIO configuration to establish
 * when Powered (POWERUP_), and when Deep Sleeping (POWERDOWN_).  The reason
 * for separate Deep Sleep settings is to allow for a slightly different
 * configuration that minimizes power consumption during Deep Sleep.  For
 * example, inputs that might float could be pulled up or down, and output
 * states chosen with care, e.g. to turn off LEDs or other devices that might
 * consume power or be unnecessary when Deep Sleeping.
 */
//@{

/** @name Packet Trace Configuration Defines
 *
 * Provide the proper set of pin configuration for when the Packet
 * Trace is enabled (look above for the define which enables it).  When Packet
 * Trace is not enabled, leave the two PTI pins in their default configuration.
 * If Packet Trace is not being used, feel free to set the pin configurations
 * as desired.  The config shown here is simply the Power On Reset defaults.
 *@{
 */
/**
 * @brief Give the packet trace configuration a friendly name.
 */
  #define PWRUP_CFG_PTI_EN    GPIOCFG_OUT_ALT
  #define PWRUP_OUT_PTI_EN    0
  #define PWRDN_CFG_PTI_EN    GPIOCFG_IN_PUD
  #define PWRDN_OUT_PTI_EN    GPIOOUT_PULLDOWN
  #define PWRUP_CFG_PTI_DATA  GPIOCFG_OUT_ALT
  #define PWRUP_OUT_PTI_DATA  1
  #define PWRDN_CFG_PTI_DATA  GPIOCFG_IN_PUD
  #define PWRDN_OUT_PTI_DATA  GPIOOUT_PULLUP
//@} END OF Packet Trace Configuration Defines

/** @name GPIO Configuration Macros
 *
 * These macros define the GPIO configuration and initial state of the output
 * registers for all the GPIO in the powerup and powerdown modes.
 *@{
 */

//Each pin has 4 cfg bits.  There are 3 ports with 2 cfg registers per
//port since the cfg register only holds 2 pins (16bits).  Therefore,
//the cfg arrays need to be 6 entries of 16bits.
extern uint16_t gpioCfgPowerUp[6];
extern uint16_t gpioCfgPowerDown[6];
//Each pin has 1 out bit.  There are 3 ports with 1 out register per
//port (8bits).  Therefore, the out arrays need to be 3 entries of 8bits.
extern uint8_t gpioOutPowerUp[3];
extern uint8_t gpioOutPowerDown[3];
//A single mask variable covers all GPIO.
extern GpioMaskType gpioRadioPowerBoardMask;

/**
 * @brief Define the mask for GPIO relevant to the radio in the context
 * of power state.  Each bit in the mask indicates the corresponding GPIO
 * which should be affected when invoking halStackRadioPowerUpBoard() or
 * halStackRadioPowerDownBoard().
 */
#define DEFINE_GPIO_RADIO_POWER_BOARD_MASK_VARIABLE() \
GpioMaskType gpioRadioPowerBoardMask = 0


/**
 * @brief Initialize GPIO powerup configuration variables.
 */
#define DEFINE_POWERUP_GPIO_CFG_VARIABLES()     \
uint16_t gpioCfgPowerUp[6] = {                                                 \
                            ((GPIOCFG_IN         <<PA0_CFG_BIT)|             \
                             (GPIOCFG_OUT_ALT    <<PA1_CFG_BIT)|             \
                             (GPIOCFG_OUT_ALT    <<PA2_CFG_BIT)|             \
                             (GPIOCFG_OUT        <<PA3_CFG_BIT)),            \
                            ((PWRDN_CFG_PTI_EN   <<PA4_CFG_BIT)|             \
                             (PWRDN_CFG_PTI_DATA <<PA5_CFG_BIT)|             \
                             (GPIOCFG_OUT        <<PA6_CFG_BIT)|             \
                             (GPIOCFG_OUT        <<PA7_CFG_BIT)),            \
                            ((GPIOCFG_OUT        <<PB0_CFG_BIT)|             \
                             (GPIOCFG_OUT        <<PB1_CFG_BIT)| /* SC1TXD */\
                             (GPIOCFG_IN         <<PB2_CFG_BIT)| /* SC1RXD */\
                             (GPIOCFG_OUT        <<PB3_CFG_BIT)),/* SC1nCTS*/\
                            ((GPIOCFG_OUT        <<PB4_CFG_BIT)| /* SC1nRTS*/\
                             (GPIOCFG_OUT        <<PB5_CFG_BIT)|             \
                             (GPIOCFG_IN_PUD     <<PB6_CFG_BIT)|             \
                             /* need to use pulldown for sleep */            \
                             (GPIOCFG_IN_PUD     <<PB7_CFG_BIT)),            \
                            ((GPIOCFG_OUT_ALT    <<PC0_CFG_BIT)|             \
                             (GPIOCFG_IN_PUD     <<PC1_CFG_BIT)|             \
                             (GPIOCFG_OUT_ALT    <<PC2_CFG_BIT)|             \
                             (GPIOCFG_IN         <<PC3_CFG_BIT)),            \
                            ((GPIOCFG_IN         <<PC4_CFG_BIT)|             \
                             (GPIOCFG_OUT        <<PC5_CFG_BIT)|             \
                             (GPIOCFG_OUT        <<PC6_CFG_BIT)|             \
                             (GPIOCFG_OUT        <<PC7_CFG_BIT))             \
                           }
/**
 * @brief Initialize GPIO powerup output variables.
 */
#define DEFINE_POWERUP_GPIO_OUTPUT_DATA_VARIABLES()                       \
uint8_t gpioOutPowerUp[3] = {                                               \
                           ((0                  <<PA0_BIT)|               \
                            (0                  <<PA1_BIT)|               \
                            (0                  <<PA2_BIT)|               \
                            /* nSSEL is default idle high */              \
                            (1                  <<PA3_BIT)|               \
                            (PWRUP_OUT_PTI_EN   <<PA4_BIT)|               \
                            (PWRUP_OUT_PTI_DATA <<PA5_BIT)|               \
                            (1                  <<PA6_BIT)|               \
                            /* LED default off */                         \
                            (1                  <<PA7_BIT)),              \
                           ((1                  <<PB0_BIT)|               \
                            (1                  <<PB1_BIT)|  /* SC1TXD  */\
                            (1                  <<PB2_BIT)|  /* SC1RXD  */\
                            (1                  <<PB3_BIT)|  /* SC1nCTS */\
                            (0                  <<PB4_BIT)|  /* SC1nRTS */\
                            (0                  <<PB5_BIT)|               \
                            /* PB6 has button needing a pullup */         \
                            (GPIOOUT_PULLUP     <<PB6_BIT)|               \
                            (0                  <<PB7_BIT)),              \
                           ((1                  <<PC0_BIT)|               \
                            (GPIOOUT_PULLUP     <<PC1_BIT)|               \
                            (1                  <<PC2_BIT)|               \
                            (0                  <<PC3_BIT)|               \
                            (0                  <<PC4_BIT)|               \
                            (1                  <<PC5_BIT)|               \
                            (0                  <<PC6_BIT)|               \
                            (0                  <<PC7_BIT))               \
                          }


/**
 * @brief Initialize powerdown GPIO configuration variables.
 */
#define DEFINE_POWERDOWN_GPIO_CFG_VARIABLES()                                  \
uint16_t gpioCfgPowerDown[6] = {                                                 \
                              ((GPIOCFG_IN_PUD     <<PA0_CFG_BIT)|             \
                               (GPIOCFG_IN_PUD     <<PA1_CFG_BIT)|             \
                               (GPIOCFG_IN_PUD     <<PA2_CFG_BIT)|             \
                               (GPIOCFG_OUT        <<PA3_CFG_BIT)),            \
                              ((PWRDN_CFG_PTI_EN   <<PA4_CFG_BIT)|             \
                               (PWRDN_CFG_PTI_DATA <<PA5_CFG_BIT)|             \
                               (GPIOCFG_OUT        <<PA6_CFG_BIT)|             \
                               (GPIOCFG_OUT        <<PA7_CFG_BIT)),            \
                              ((GPIOCFG_OUT        <<PB0_CFG_BIT)|             \
                               (GPIOCFG_OUT        <<PB1_CFG_BIT)| /* SC1TXD */\
                               (GPIOCFG_IN_PUD     <<PB2_CFG_BIT)| /* SC1RXD */\
                               (GPIOCFG_IN_PUD     <<PB3_CFG_BIT)),/* SC1nCTS*/\
                              ((GPIOCFG_OUT        <<PB4_CFG_BIT)| /* SC1nRTS*/\
                               (GPIOCFG_OUT        <<PB5_CFG_BIT)|             \
                               (GPIOCFG_IN_PUD     <<PB6_CFG_BIT)|             \
                               /* need to use pulldown for sleep */            \
                               (GPIOCFG_IN_PUD     <<PB7_CFG_BIT)),            \
                              ((GPIOCFG_IN_PUD     <<PC0_CFG_BIT)|             \
                               (GPIOCFG_IN_PUD     <<PC1_CFG_BIT)|             \
                               (GPIOCFG_OUT        <<PC2_CFG_BIT)|             \
                               (GPIOCFG_IN_PUD     <<PC3_CFG_BIT)),            \
                              ((GPIOCFG_IN_PUD     <<PC4_CFG_BIT)|             \
                               (GPIOCFG_OUT_ALT    <<PC5_CFG_BIT)|             \
                               (GPIOCFG_OUT        <<PC6_CFG_BIT)|             \
                               (GPIOCFG_OUT        <<PC7_CFG_BIT))             \
                             }


/**
 * @brief Initialize powerdown GPIO output variables.
 */
#define DEFINE_POWERDOWN_GPIO_OUTPUT_DATA_VARIABLES()                       \
uint8_t gpioOutPowerDown[3] = {                                               \
                             ((GPIOOUT_PULLUP     <<PA0_BIT)|               \
                              (GPIOOUT_PULLUP     <<PA1_BIT)|               \
                              (GPIOOUT_PULLUP     <<PA2_BIT)|               \
                              /* nSSEL is idle high */                      \
                              (1                  <<PA3_BIT)|               \
                              /* enable is idle low */                      \
                              (PWRDN_OUT_PTI_EN   <<PA4_BIT)|               \
                              /* data is idle high */                       \
                              (PWRDN_OUT_PTI_DATA <<PA5_BIT)|               \
                              (1                  <<PA6_BIT)|/*STATUS LED*/ \
                              (1                  <<PA7_BIT)),/*POWER LED*/ \
                             ((0                  <<PB0_BIT)|               \
                              (GPIOOUT_PULLUP     <<PB1_BIT)|  /* SC1TXD  */\
                              (GPIOOUT_PULLUP     <<PB2_BIT)|  /* SC1RXD  */\
                              (GPIOOUT_PULLDOWN   <<PB3_BIT)|  /* SC1nCTS */\
                              (GPIOOUT_PULLUP     <<PB4_BIT)|  /* SC1nRTS */\
                              (1                  <<PB5_BIT)|               \
                              /* PB6 has button needing a pullup */         \
                              (GPIOOUT_PULLUP     <<PB6_BIT)|               \
                              /* buzzer needs pulldown for sleep */         \
                              (GPIOOUT_PULLDOWN   <<PB7_BIT)),              \
                             ((GPIOOUT_PULLUP     <<PC0_BIT)|               \
                              (GPIOCFG_IN_PUD     <<PC1_BIT)|               \
                              (1                  <<PC2_BIT)|               \
                              (GPIOOUT_PULLDOWN   <<PC3_BIT)|               \
                              (GPIOOUT_PULLDOWN   <<PC4_BIT)|               \
                              (1                  <<PC5_BIT)|               \
                              (0                  <<PC6_BIT)|               \
                              (1                  <<PC7_BIT))               \
                            }


/**
 * @brief Set powerup GPIO configuration registers.
 */
#define SET_POWERUP_GPIO_CFG_REGISTERS() \
  GPIO_PACFGL = gpioCfgPowerUp[0];       \
  GPIO_PACFGH = gpioCfgPowerUp[1];       \
  GPIO_PBCFGL = gpioCfgPowerUp[2];       \
  GPIO_PBCFGH = gpioCfgPowerUp[3];       \
  GPIO_PCCFGL = gpioCfgPowerUp[4];       \
  GPIO_PCCFGH = gpioCfgPowerUp[5];

/**
 * @brief Set powerup GPIO output registers.
 */
#define SET_POWERUP_GPIO_OUTPUT_DATA_REGISTERS() \
  GPIO_PAOUT = gpioOutPowerUp[0];                \
  GPIO_PBOUT = gpioOutPowerUp[1];                \
  GPIO_PCOUT = gpioOutPowerUp[2];

/**
 * @brief Set powerdown GPIO configuration registers.
 */
#define SET_POWERDOWN_GPIO_CFG_REGISTERS() \
  GPIO_PACFGL = gpioCfgPowerDown[0];       \
  GPIO_PACFGH = gpioCfgPowerDown[1];       \
  GPIO_PBCFGL = gpioCfgPowerDown[2];       \
  GPIO_PBCFGH = gpioCfgPowerDown[3];       \
  GPIO_PCCFGL = gpioCfgPowerDown[4];       \
  GPIO_PCCFGH = gpioCfgPowerDown[5];

/**
 * @brief Set powerdown GPIO output registers.
 */
#define SET_POWERDOWN_GPIO_OUTPUT_DATA_REGISTERS() \
  GPIO_PAOUT = gpioOutPowerDown[0];                \
  GPIO_PBOUT = gpioOutPowerDown[1];                \
  GPIO_PCOUT = gpioOutPowerDown[2];

/**
 * @brief External regulator enable/disable macro.
 */
#ifdef ENABLE_ALT_FUNCTION_REG_EN
  #define CONFIGURE_EXTERNAL_REGULATOR_ENABLE()  GPIO_DBGCFG |= GPIO_EXTREGEN;
#else
  #define CONFIGURE_EXTERNAL_REGULATOR_ENABLE()  GPIO_DBGCFG &= ~GPIO_EXTREGEN;
#endif
//@} END OF GPIO Configuration Macros

/** @name GPIO Wake Source Definitions
 *
 * A convenient define that chooses if this external signal can
 * be used as source to wake from deep sleep.  Any change in the state of the
 * signal will wake up the CPU.
 */
 //@{
/**
 * @brief true if this GPIO can wake the chip from deep sleep, false if not.
 */
#define WAKE_ON_PA0   false
#define WAKE_ON_PA1   false
#define WAKE_ON_PA2   false
#define WAKE_ON_PA3   false
#define WAKE_ON_PA4   false
#define WAKE_ON_PA5   false
#define WAKE_ON_PA6   false
#define WAKE_ON_PA7   false
#define WAKE_ON_PB0   false
#define WAKE_ON_PB1   false
#define WAKE_ON_PB2   false
#define WAKE_ON_PB3   false
#define WAKE_ON_PB4   false
#define WAKE_ON_PB5   false
#define WAKE_ON_PB6   false
#define WAKE_ON_PB7   false
#define WAKE_ON_PC0   false
#define WAKE_ON_PC1   false
#define WAKE_ON_PC2   false
#define WAKE_ON_PC3   false
#define WAKE_ON_PC4   false
#define WAKE_ON_PC5   false
#define WAKE_ON_PC6   false
#define WAKE_ON_PC7   false
//@} //END OF GPIO Wake Source Definitions

//@} //END OF GPIO Configuration Definitions

/** @name Board Specific Functions
 *
 * The following macros exist to aid in the initialization, power up from sleep,
 * and power down to sleep operations.  These macros are responsible for
 * either initializing directly, or calling initialization functions for any
 * peripherals that are specific to this board implementation.  These
 * macros are called from halInit, halPowerDown, and halPowerUp respectively.
 */
 //@{
/**
 * @brief Initialize the board.  This function is called from ::halInit().
 */
  #define halInternalInitBoard()                                  \
          do {                                                    \
            halInternalPowerUpBoard();                            \
            halInternalRestartUart();                             \
            halInternalInitButton();                              \
         } while(0)

/**
 * @brief Power down the board.  This function is called from
 * ::halPowerDown().
 */
#define halInternalPowerDownBoard()                   \
        do {                                          \
          /* Board peripheral deactivation */         \
          /* halInternalSleepAdc(); */                \
          SET_POWERDOWN_GPIO_OUTPUT_DATA_REGISTERS()  \
          SET_POWERDOWN_GPIO_CFG_REGISTERS()          \
        } while(0)

/**
 * @brief Power up the board.  This function is called from
 * ::halPowerUp().
 */
#define halInternalPowerUpBoard()                                  \
        do {                                                       \
          SET_POWERUP_GPIO_OUTPUT_DATA_REGISTERS()                 \
          SET_POWERUP_GPIO_CFG_REGISTERS()                         \
          /*The radio GPIO should remain in the powerdown state */ \
          /*until the stack specifically powers them up. */        \
          halStackRadioPowerDownBoard();                           \
          CONFIGURE_EXTERNAL_REGULATOR_ENABLE()                    \
          /* Board peripheral reactivation */                      \
          halInternalInitAdc();                                    \
        } while(0)
//@} //END OF BOARD SPECIFIC FUNCTIONS

/**
 * @brief Suspend the board.  This function is called from
 * ::halSuspend().
 */
#define halInternalSuspendBoard() //nop for smart outlet

/**
 * @brief Resume the board.  This function is called from
 * ::halResume().
 */
#define halInternalResumeBoard() //nop for smart outlet 
//@} //END OF BOARD SPECIFIC FUNCTIONS

#endif //__BOARD_H__

/** @} END Board Specific Functions */

/** @} END addtogroup */

