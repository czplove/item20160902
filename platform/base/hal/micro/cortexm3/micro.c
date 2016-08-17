/*
 * File: micro.c
 * Description: EM3XX micro specific full HAL functions
 *
 *
 * Copyright 2008, 2009 by Ember Corporation. All rights reserved.          *80*
 */
//[[ Author(s): Brooks Barrett, Lee Taylor ]]


#include PLATFORM_HEADER
#include "stack/include/ember.h"
#include "include/error.h"

#include "hal/hal.h"
#include "serial/serial.h"
#include "hal/micro/cortexm3/diagnostic.h"
#include "hal/micro/cortexm3/memmap.h"
#include "hal/micro/cortexm3/flash.h"
#include "stack/platform/micro/debug-channel.h"

#ifdef RTOS
  #include "rtos/rtos.h"
#endif

static void halStackRadioHoldOffPowerDown(void); // fwd ref
static void halStackRadioHoldOffPowerUp(void);   // fwd ref

// halInit is called on first initial boot, not on wakeup from sleep.
void halInit(void)
{
  //[[ Strip emulator only code from official build
  #ifdef EMBER_EMU_TEST
    //On the emulator, give our fake XTAL reasonable thresholds so the cal
    //algorithm ends up at 4.
    EMU_OSC24M_CTRL =((0x8<<EMU_OSC24M_CTRL_OSC24M_THRESH_H_BIT) |
                      (0x2<<EMU_OSC24M_CTRL_OSC24M_THRESH_L_BIT) |
                      (0x0<<EMU_OSC24M_CTRL_OSC24M_THRESH_STOP_BIT));
  #endif
  //]]
  
  halCommonStartXtal();
  
  halInternalSetRegTrim(false);
  
  GPIO_DBGCFG |= GPIO_DBGCFGRSVD;
  
  #ifndef DISABLE_WATCHDOG
    halInternalEnableWatchDog();
  #endif
  
  halCommonCalibratePads();

  #ifdef DISABLE_INTERNAL_1V8_REGULATOR
    //Out of reset, VREG 1V8 is enabled.  Update the helper variable
    //to keep the state in agreement with what the disable API is expecting.
    halCommonVreg1v8EnableCount = 1;
    halCommonDisableVreg1v8();
  #endif
  
  halInternalInitBoard();
  
  halCommonSwitchToXtal();
  
  #ifndef DISABLE_RC_CALIBRATION
    halInternalCalibrateFastRc();
  #endif//DISABLE_RC_CALIBRATION
  
  halInternalStartSystemTimer();
  
  #ifdef INTERRUPT_DEBUGGING
    //When debugging interrupts/ATOMIC, ensure that our
    //debug pin is properly enabled and idle low.
    I_OUT(I_PORT, I_PIN, I_CFG_HL);
    I_CLR(I_PORT, I_PIN);
  #endif //INTERRUPT_DEBUGGING
  
  #ifdef USB_CERT_TESTING
  halInternalPowerDownBoard();
  #endif
}

void halReboot(void)
{
  halInternalSysReset(RESET_SOFTWARE_REBOOT);
}

void halPowerDown(void)
{
  emDebugPowerDown();

  halInternalPowerDownUart();
  
  halInternalPowerDownBoard();
}

// halPowerUp is called from sleep state, not from first initial boot.
void halPowerUp(void)
{
  //[[ Strip emulator only code from official build
  #ifdef EMBER_EMU_TEST
    //On the emulator, give our fake XTAL reasonable thresholds so the cal
    //algorithm ends up at 4.
    EMU_OSC24M_CTRL =((0x8<<EMU_OSC24M_CTRL_OSC24M_THRESH_H_BIT) |
                      (0x2<<EMU_OSC24M_CTRL_OSC24M_THRESH_L_BIT) |
                      (0x0<<EMU_OSC24M_CTRL_OSC24M_THRESH_STOP_BIT));
  #endif
  //]]

  halInternalPowerUpKickXtal();

  halCommonCalibratePads();

  //NOTE: The register VREG is a high voltage register that holds its
  //      state across deep sleep.  While halInit() must be sensitive
  //      to the define DISABLE_INTERNAL_1V8_REGULATOR, halPowerUp() 
  //      trusts that VREG hasn't been modified and therefore doesn't
  //      need to take further action with respect to VREG.
  
  halInternalPowerUpBoard();

  halInternalBlockUntilXtal();

  halInternalPowerUpUart();  

  emDebugPowerUp();
}

// halSuspend suspends all board activity except for USB
void halSuspend(void)
{
  halInternalPowerDownUart();
  
  halInternalSuspendBoard();
}

// halResume restores all board activity from a previous USB suspend
void halResume(void)
{
  //[[ Strip emulator only code from official build
  #ifdef EMBER_EMU_TEST
    //On the emulator, give our fake XTAL reasonable thresholds so the cal
    //algorithm ends up at 4.
    EMU_OSC24M_CTRL =((0x8<<EMU_OSC24M_CTRL_OSC24M_THRESH_H_BIT) |
                      (0x2<<EMU_OSC24M_CTRL_OSC24M_THRESH_L_BIT) |
                      (0x0<<EMU_OSC24M_CTRL_OSC24M_THRESH_STOP_BIT));
  #endif
  //]]

  halInternalPowerUpKickXtal();

  halCommonCalibratePads();
  
  halInternalResumeBoard();

  halInternalBlockUntilXtal();

  halInternalPowerUpUart();
}

//If the board file defines runtime powerup/powerdown GPIO configuration,
//instantiate the variables and implement halStackRadioPowerDownBoard/
//halStackRadioPowerUpBoard which the stack will use to control the
//power state of radio specific GPIO.  If the board file does not define
//runtime GPIO configuration, the compile time configuration will work as
//it always has.
#if defined(DEFINE_POWERUP_GPIO_CFG_VARIABLES)           && \
    defined(DEFINE_POWERUP_GPIO_OUTPUT_DATA_VARIABLES)   && \
    defined(DEFINE_POWERDOWN_GPIO_CFG_VARIABLES)         && \
    defined(DEFINE_POWERDOWN_GPIO_OUTPUT_DATA_VARIABLES) && \
    defined(DEFINE_GPIO_RADIO_POWER_BOARD_MASK_VARIABLE)


DEFINE_POWERUP_GPIO_CFG_VARIABLES();
DEFINE_POWERUP_GPIO_OUTPUT_DATA_VARIABLES();
DEFINE_POWERDOWN_GPIO_CFG_VARIABLES();
DEFINE_POWERDOWN_GPIO_OUTPUT_DATA_VARIABLES();
DEFINE_GPIO_RADIO_POWER_BOARD_MASK_VARIABLE();
       

static void rmwRadioPowerCfgReg(uint16_t radioPowerCfg[],
                                uint32_t volatile * cfgReg,
                                uint8_t cfgVar)
{
  uint32_t temp = *cfgReg;
  uint8_t i;
  
  //don't waste time with a register that doesn't have anything to be done
  if(gpioRadioPowerBoardMask&(((GpioMaskType)0xF)<<(4*cfgVar))) {
    //loop over the 4 pins of the cfgReg
    for(i=0; i<4; i++) {
      if((gpioRadioPowerBoardMask>>((4*cfgVar)+i))&1) {
        //read-modify-write the pin's cfg if the mask says it pertains
        //to the radio's power state
        temp &= ~(0xFu<<(4*i));
        temp |= (radioPowerCfg[cfgVar]&(0xF<<(4*i)));
      }
    }
  }
  
  *cfgReg = temp;
}


static void rmwRadioPowerOutReg(uint8_t radioPowerOut[],
                                uint32_t volatile * outReg,
                                uint8_t outVar)
{
  uint32_t temp = *outReg;
  uint8_t i;
  
  //don't waste time with a register that doesn't have anything to be done
  if(gpioRadioPowerBoardMask&(((GpioMaskType)0xFF)<<(8*outVar))) {
    //loop over the 8 pins of the outReg
    for(i=0; i<8; i++) {
      if((gpioRadioPowerBoardMask>>((8*outVar)+i))&1) {
        //read-modify-write the pin's out if the mask says it pertains
        //to the radio's power state
        temp &= ~(0x1u<<(1*i));
        temp |= (radioPowerOut[outVar]&(0x1<<(1*i)));
      }
    }
  }
  
  *outReg = temp;
}

void halStackRadioPowerDownBoard(void)
{
  (void) halPtaSetRxRequest(PTA_REQ_OFF, NULL);
  // Don't touch PTA Tx Request here
  halStackRadioHoldOffPowerDown();
  if(gpioRadioPowerBoardMask == 0) {
    //If the mask indicates there are no special GPIOs for the
    //radio that need their power state to be conrolled by the stack,
    //don't bother attempting to do anything.
    return;
  }
  
  rmwRadioPowerOutReg(gpioOutPowerDown, &GPIO_PAOUT, 0);
  rmwRadioPowerOutReg(gpioOutPowerDown, &GPIO_PBOUT, 1);
  rmwRadioPowerOutReg(gpioOutPowerDown, &GPIO_PCOUT, 2);
  
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PACFGL, 0);
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PACFGH, 1);
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PBCFGL, 2);
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PBCFGH, 3);
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PCCFGL, 4);
  rmwRadioPowerCfgReg(gpioCfgPowerDown, &GPIO_PCCFGH, 5);
}

void halStackRadioPowerUpBoard(void)
{
  halStackRadioHoldOffPowerUp();
  if(gpioRadioPowerBoardMask == 0) {
    //If the mask indicates there are no special GPIOs for the
    //radio that need their power state to be conrolled by the stack,
    //don't bother attempting to do anything.
    return;
  }
  
  rmwRadioPowerOutReg(gpioOutPowerUp, &GPIO_PAOUT, 0);
  rmwRadioPowerOutReg(gpioOutPowerUp, &GPIO_PBOUT, 1);
  rmwRadioPowerOutReg(gpioOutPowerUp, &GPIO_PCOUT, 2);
  
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PACFGL, 0);
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PACFGH, 1);
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PBCFGL, 2);
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PBCFGH, 3);
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PCCFGL, 4);
  rmwRadioPowerCfgReg(gpioCfgPowerUp, &GPIO_PCCFGH, 5);
}

#else

//If the board file uses traditional compile time powerup/powerdown GPIO
//configuration, stub halStackRadioPowerDownBoard/halStackRadioPowerUpBoard
//which the stack would have used to control the power state of radio
//relevant GPIO.  With compile time configuration, only the traditional
//halInternalPowerDownBoard and halInternalPowerUpBoard funtions configure
//the GPIO.  RHO powerdown/up still needs to be managed however.

void halStackRadioPowerDownBoard(void)
{
  (void) halPtaSetRxRequest(PTA_REQ_OFF, NULL);
  // Don't touch PTA Tx Request here
  halStackRadioHoldOffPowerDown();
}

void halStackRadioPowerUpBoard(void)
{
  halStackRadioHoldOffPowerUp();
}

#endif

void halStackRadioPowerMainControl(bool powerUp)
{
  if (powerUp) {
    halRadioPowerUpHandler();
  } else {
    halRadioPowerDownHandler();
  }
}

void halStackProcessBootCount(void)
{
  //Note: Becuase this always counts up at every boot (called from emberInit),
  //and non-volatile storage has a finite number of write cycles, this will
  //eventually stop working.  Disable this token call if non-volatile write
  //cycles need to be used sparingly.
  halCommonIncrementCounterToken(TOKEN_STACK_BOOT_COUNTER);
}

PGM_P halGetResetString(void)
{
  // Table used to convert from reset types to reset strings.
  #define RESET_BASE_DEF(basename, value, string)  string,
  #define RESET_EXT_DEF(basename, extname, extvalue, string)  /*nothing*/
  static PGM char resetStringTable[][4] = {
    #include "reset-def.h"
  };
  #undef RESET_BASE_DEF
  #undef RESET_EXT_DEF

  return resetStringTable[halGetResetInfo()];
}

// Note that this API should be used in conjunction with halGetResetString
//  to get the full information, as this API does not provide a string for
//  the base reset type
PGM_P halGetExtendedResetString(void)
{
  // Create a table of reset strings for each extended reset type
  typedef PGM char ResetStringTableType[][4];
  #define RESET_BASE_DEF(basename, value, string)   \
                         }; static ResetStringTableType basename##ResetStringTable = {
  #define RESET_EXT_DEF(basename, extname, extvalue, string)  string,
  {
    #include "reset-def.h"
  };
  #undef RESET_BASE_DEF
  #undef RESET_EXT_DEF
  
  // Create a table of pointers to each of the above tables
  #define RESET_BASE_DEF(basename, value, string)  (ResetStringTableType *)basename##ResetStringTable,
  #define RESET_EXT_DEF(basename, extname, extvalue, string)  /*nothing*/
  static ResetStringTableType * PGM extendedResetStringTablePtrs[] = {
    #include "reset-def.h"
  };
  #undef RESET_BASE_DEF
  #undef RESET_EXT_DEF

  uint16_t extResetInfo = halGetExtendedResetInfo();
  // access the particular table of extended strings we are interested in
  ResetStringTableType *extendedResetStringTable = 
                    extendedResetStringTablePtrs[RESET_BASE_TYPE(extResetInfo)];

  // return the string from within the proper table
  return (*extendedResetStringTable)[((extResetInfo)&0xFF)];
}

// Translate EM3xx reset codes to the codes previously used by the EM2xx.
// If there is no corresponding code, return the EM3xx base code with bit 7 set.
uint8_t halGetEm2xxResetInfo(void)
{
  uint8_t reset = halGetResetInfo();

  // Any reset with an extended value field of zero is considered an unknown
  // reset, except for FIB resets.
  if ( (RESET_EXTENDED_FIELD(halGetExtendedResetInfo()) == 0) && 
       (reset != RESET_FIB) ) {
     return EM2XX_RESET_UNKNOWN;
  }

 switch (reset) {
  case RESET_UNKNOWN:
    return EM2XX_RESET_UNKNOWN;
  case RESET_BOOTLOADER:
    return EM2XX_RESET_BOOTLOADER;
  case RESET_EXTERNAL:      // map pin resets to poweron for EM2xx compatibility
//    return EM2XX_RESET_EXTERNAL;  
  case RESET_POWERON:
    return EM2XX_RESET_POWERON;
  case RESET_WATCHDOG:
    return EM2XX_RESET_WATCHDOG;
  case RESET_SOFTWARE:
    return EM2XX_RESET_SOFTWARE;
  case RESET_CRASH:
    return EM2XX_RESET_ASSERT;
  default:
    return (reset | 0x80);      // set B7 for all other reset codes
  }
}

  // halPta Implementation:

  // Board header is expected to define:
  // PTA REQUEST signal (OUT or OUT_OD): [optional]
  // #define PTA_REQ_GPIO       // PORTx_PIN(y) (x=A/B/C, y=0-7)
  // #define PTA_REQ_GPIOCFG_NORMAL // GPIOCFG_OUT
  // #define PTA_REQ_GPIOCFG_SHARED // GPIOCFG_OUT_OD (_ASSERTED must be 0)
  // #define PTA_REQ_GPIOCFG    // PTA_REQ_GPIOCFG_NORMAL or _SHARED as above
  // #define PTA_REQ_GPIOCFG    // GPIOCFG_OUT or GPIOCFG_OUT_OD if shared
  // #define PTA_REQ_ASSERTED   // 0 if negated logic; 1 if positive logic
  // #define PTA_REQ_FLAG_BIT   // INT_IRQnFLAG (n=A/B/C/D) [only if shared]
  // #define PTA_REQ_MISS_BIT   // INT_MISSIRQn (n=A/B/C/D) [only if shared]
  // #define PTA_REQ_INTCFG     // GPIO_INTCFGn (n=A/B/C/D) [only if shared]
  // #define PTA_REQ_INT_EN_BIT // INT_IRQn     (n=A/B/C/D) [only if shared]
  // #define PTA_REQ_SEL()      // do { GPIO_IRQnSEL = PTA_REQ_GPIO; } while (0)
  //                            // (n=C/D or empty if n=A/B)[only if shared]
  //
  // PTA GRANT signal (IN): [optional]
  // #define PTA_GNT_GPIO       // PORTx_PIN(y) (x=A/B/C, y=0-7)
  // #define PTA_GNT_GPIOCFG    // GPIOCFG_IN_PUD[ASSERTED] or GPIOCFG_IN
  // #define PTA_GNT_ASSERTED   // 0 if negated logic; 1 if positive logic
  // #define PTA_GNT_FLAG_BIT   // INT_IRQnFLAG (n=A/B/C/D)
  // #define PTA_GNT_MISS_BIT   // INT_MISSIRQn (n=A/B/C/D)
  // #define PTA_GNT_INTCFG     // GPIO_INTCFGn (n=A/B/C/D)
  // #define PTA_GNT_INT_EN_BIT // INT_IRQn     (n=A/B/C/D)
  // #define PTA_GNT_SEL()      // do { GPIO_IRQnSEL = PTA_GNT_GPIO; } while (0)
  //                            // (n=C/D or empty if n=A/B)
  // Note that REQ and GNT can share the same IRQn if necessary
  //
  // PTA PRIORITY signal (OUT): [optional]
  // #define PTA_PRI_GPIO       // PORTx_PIN(y) (x=A/B/C, y=0-7)
  // #define PTA_PRI_GPIOCFG    // GPIOCFG_OUT or GPIOCFG_OUT_OD if shared
  // #define PTA_PRI_ASSERTED   // 0 if negated logic; 1 if positive logic

 #ifdef  PTA_REQ_GPIO

  static bool ptaReqAsserted = PTA_REQ_ASSERTED;

  static inline void ptaReqGpioSet(bool enable)
  {
    if (enable == ptaReqAsserted) {
      halGpioSet(PTA_REQ_GPIO);
    } else {
      halGpioClear(PTA_REQ_GPIO);
    }
  }

  static inline void ptaReqGpioCfg(void)
  {
    // GPIO must have been set up prior.
    // Here we sense asserted state is opposite of its current output state.
    ptaReqAsserted = !halGpioReadOutput(PTA_REQ_GPIO);
  }

  #define ptaReqGpioCfgIsShared() \
            (halGpioGetConfig(PTA_REQ_GPIO) != PTA_REQ_GPIOCFG_NORMAL)

  #define ptaReqGpioInAsserted() \
            (!!halGpioRead(PTA_REQ_GPIO) == !!ptaReqAsserted)

  #define ptaReqGpioOutAsserted() \
            (!!halGpioReadOutput(PTA_REQ_GPIO) == !!ptaReqAsserted)

  #define ptaReqAndGntIrqShared() \
            ( (defined(PTA_GNT_GPIO))                          /*Have GNT*/ \
            &&(PTA_REQ_INT_EN_BIT == PTA_GNT_INT_EN_BIT) )     /*Shared IRQ*/

  static inline void ptaReqGpioIntAcknowledge(void)
  {
    INT_MISS = PTA_REQ_MISS_BIT;
    INT_GPIOFLAG = PTA_REQ_FLAG_BIT;
  }

  static inline void ptaReqGpioIntDisable(void)
  {
    INT_CFGCLR = PTA_REQ_INT_EN_BIT;  //clear top level int enable
    INT_PENDCLR = PTA_REQ_INT_EN_BIT; //clear any pended event
    ptaReqGpioIntAcknowledge();
  }

  static inline void ptaReqGpioIntEnable(void)
  {
    PTA_REQ_INTCFG = 0;               //disable triggering
    ptaReqGpioIntDisable();           //clear any stale events
    PTA_REQ_INTCFG  = (0 << GPIO_INTFILT_BIT) /* 0 = no filter  */
                    | ((GPIOINTMOD_RISING_EDGE+ptaReqAsserted) << GPIO_INTMOD_BIT); /* deassert edge */
    PTA_REQ_SEL(); //point IRQ to the desired pin
    INT_CFGSET = PTA_REQ_INT_EN_BIT;  //set top level interrupt enable
  }

 #else//!PTA_REQ_GPIO

  #define ptaReqGpioSet(enable)      /* no-op */
  #define ptaReqGpioCfg()            /* no-op */
  #define ptaReqGpioCfgIsShared()    0
  #define ptaReqGpioInAsserted()     0
  #define ptaReqGpioIntAcknowledge() /* no-op */
  #define ptaReqGpioIntDisable()     /* no-op */
  #define ptaReqGpioIntEnable()      /* no-op */
  #define ptaReqGpioOutAsserted()    1
  #define ptaReqAndGntIrqShared()    0

 #endif//PTA_REQ_GPIO

 #ifdef  PTA_GNT_GPIO

  void PTA_GNT_ISR(void);

  static bool ptaGntAsserted = PTA_GNT_ASSERTED;
  static bool gntWasAsserted = false;

  static inline void ptaGntGpioCfg(void)
  {
    // GPIO must have been set up prior.
    // Here we sense asserted state is same as its current output state.
    ptaGntAsserted = !!halGpioReadOutput(PTA_GNT_GPIO);
  }

  #define ptaGntGpioInAsserted() \
            (!!halGpioRead(PTA_GNT_GPIO) == !!ptaGntAsserted)

  static inline void ptaGntGpioIntAcknowledge(void)
  {
    INT_MISS = PTA_GNT_MISS_BIT;
    INT_GPIOFLAG = PTA_GNT_FLAG_BIT;
  }

  static inline void ptaGntGpioIntDisable(void)
  {
    INT_CFGCLR = PTA_GNT_INT_EN_BIT;  //clear top level int enable
    INT_PENDCLR = PTA_GNT_INT_EN_BIT; //clear any pended event
    ptaGntGpioIntAcknowledge();
  }

  static inline void ptaGntGpioIntEnable(void)
  {
    PTA_GNT_INTCFG = 0;               //disable triggering
    ptaGntGpioIntDisable();           //clear any stale events
    gntWasAsserted = false; // Ensures we won't miss GNT assertion
    PTA_GNT_INTCFG  = (0 << GPIO_INTFILT_BIT) /* 0 = no filter  */
                    | (GPIOINTMOD_BOTH_EDGES << GPIO_INTMOD_BIT);
    PTA_GNT_SEL(); //point IRQ to the desired pin
    INT_CFGSET = PTA_GNT_INT_EN_BIT;  //set top level interrupt enable
  }

  static inline void ptaGntGpioIntPend(void)
  {
    INT_PENDSET = PTA_GNT_INT_EN_BIT;
  }

 #else//!PTA_GNT_GPIO

  #define ptaGntGpioCfg()            /* no-op */
  #define ptaGntGpioInAsserted()     1
  #define ptaGntGpioIntAcknowledge() /* no-op */
  #define ptaReqGpioIntDisable()     /* no-op */
  #define ptaGntGpioIntDisable()     /* no-op */
  #define ptaGntGpioIntEnable()      /* no-op */
  #define ptaGntGpioIntPend()        (halInternalPtaOrRhoNotifyRadio())

 #endif//PTA_GNT_GPIO

 #ifdef  PTA_PRI_GPIO

  static bool ptaPriAsserted = PTA_PRI_ASSERTED;

  static inline void ptaPriGpioSet(bool enable)
  {
    if (enable != ptaPriAsserted) {
      halGpioClear(PTA_PRI_GPIO);
    } else {
      halGpioSet(PTA_PRI_GPIO);
    }
  }

  static inline void ptaPriGpioCfg(void)
  {
    // GPIO must have been set up prior.
    // Here we sense asserted state is opposite of its current output state.
    ptaPriAsserted = !halGpioReadOutput(PTA_PRI_GPIO);
  }

 #else//!PTA_PRI_GPIO

  #define ptaPriGpioSet(enable)      /* no-op */
  #define ptaPriGpioCfg()            /* no-op */

 #endif//PTA_PRI_GPIO

  static void halInternalPtaOrRhoNotifyRadio(void);
  static volatile bool ptaEnabled;
  static volatile halPtaReq_t txReq, rxReq;
  static volatile halPtaCb_t  txCb,  rxCb;

  // Must be called with interrupts disabled
  static void ptaUpdateReqIsr(void)
  {
    halPtaReq_t txReqL = txReq; // Local non-volatile flavor avoids warnings
    halPtaReq_t rxReqL = rxReq; // Local non-volatile flavor avoids warnings
    bool myReq = !!((txReqL | rxReqL) & PTA_REQ_ON);     // I need to REQUEST
    bool force = !!((txReqL | rxReqL) & PTA_REQ_FORCE);  // (ignoring others)
    bool exReq;                                          // external requestor?
    if (ptaReqGpioOutAsserted()) {  // in GRANT phase
      exReq = false;                // ignore external requestors
    } else {                        // in REQUEST phase
      ptaReqGpioIntAcknowledge();   // Before sampling REQUEST, avoids race
      exReq = ptaReqGpioCfgIsShared() && ptaReqGpioInAsserted();
    }
    if (myReq) {                    // want to assert REQUEST
      if (force || !exReq) {        // can assert REQUEST
        ptaGntGpioIntAcknowledge();
        ptaGntGpioIntEnable();
        ptaReqGpioSet(true);
        ptaPriGpioSet(!!((txReqL | rxReqL) & PTA_REQ_HIPRI));

        // Issue callbacks on REQUEST assertion
        // These are one-shot callbacks
        if ((rxCb != NULL) && (rxReq & PTA_REQCB_REQUESTED)) {
          (*rxCb)(PTA_REQCB_REQUESTED);
          rxReq &= ~PTA_REQCB_REQUESTED;
        }
        if ((txCb != NULL) && (txReq & PTA_REQCB_REQUESTED)) {
          (*txCb)(PTA_REQCB_REQUESTED);
          txReq &= ~PTA_REQCB_REQUESTED;
        }
        ptaGntGpioIntPend(); // Manually force GRANT check if missed/no edge
      } else {                      // must wait for REQUEST
        ptaReqGpioIntEnable();
      }
    } else {                        // negate REQUEST
      ptaPriGpioSet(false);
      ptaReqGpioSet(false);
      ptaGntGpioIntDisable();
      ptaReqGpioIntDisable();
      halInternalPtaOrRhoNotifyRadio(); // Reassess (assert) RHO
    }
  }

 #ifdef  PTA_GNT_GPIO
  // Triggered on both GRANT edges
  void PTA_GNT_ISR(void)
  {
    ptaGntGpioIntAcknowledge();
    if (ptaReqGpioOutAsserted()) {  // GRANT phase
      bool newGnt = ptaGntGpioInAsserted(); // Sample GPIO once, now
      if (newGnt != gntWasAsserted) {
        gntWasAsserted = newGnt;
        halInternalPtaOrRhoNotifyRadio();
        // Issue callbacks on GRANT assert or negate
        // These are not one-shot callbacks
        halPtaReq_t newState = (newGnt ? PTA_REQCB_GRANTED : PTA_REQCB_NEGATED);
        if ((rxCb != NULL) && (rxReq & newState)) {
          (*rxCb)(newState);
        }
        if ((txCb != NULL) && (txReq & newState)) {
          (*txCb)(newState);
        }
        //Do we need this to meet GRANT -> REQUEST timing?
        // On GNT deassertion, pulse REQUEST to keep us going.
        // Don't want to revert to REQUEST phase here but stay in GRANT phase.
        // This seems dangerous in that it could allow a peer to assert their
        // REQUEST causing a conflict/race.
       #ifdef ENABLE_PTA_REQ_PULSE
        if (!newGnt) {
          ptaReqGpioSet(false);
          ptaReqGpioSet(true);
        }
       #endif
      }
    } else {                        // REQUEST phase
     #if     (ptaReqAndGntIrqShared())
      if (ptaReqGpioCfgIsShared()) {
        // External REQUEST deasserted so we can assert ours
        ptaReqGpioIntDisable(); // This is a one-shot event
        //TODO: Perform some random backoff before claiming REQUEST??
        ptaUpdateReqIsr();
      } else {
        // Ignore GRANT changes unless we are REQUESTing
      }
     #endif//(ptaReqAndGntIrqShared())
    }
  }
 #endif//PTA_GNT_GPIO

 #if     ( defined(PTA_REQ_GPIO) && (!ptaReqAndGntIrqShared()) )
  // This IRQ is triggered on the negate REQUEST edge,
  // needed only when REQUEST signal is shared,
  // and not piggybacking GNT and REQ on same IRQ.
  void PTA_REQ_ISR(void)
  {
    // External REQUEST deasserted so we can assert ours
    //ptaReqGpioIntAcknowledge(); // Covered within ptaReqGpioIntDisable()
    ptaReqGpioIntDisable(); // This is a one-shot event
    //TODO: Perform some random backoff before claiming REQUEST??
    ptaUpdateReqIsr();
  }
 #endif//( defined(PTA_REQ_GPIO) && (!ptaReqAndGntIrqShared()) )

  // Public API

 #ifndef EM_PTA_MAC_RETRANSMIT_TOGGLE_REQ
 #define EM_PTA_MAC_RETRANSMIT_TOGGLE_REQ 0 // default no toggle on retransmit
 #endif//EM_PTA_MAC_RETRANSMIT_TOGGLE_REQ

  bool halPtaToggleReqOnMacRetransmit = EM_PTA_MAC_RETRANSMIT_TOGGLE_REQ;

  EmberStatus halPtaSetTxRequest(halPtaReq_t ptaReq, halPtaCb_t ptaCb)
  {
    EmberStatus status = EMBER_ERR_FATAL;
    ATOMIC(
      if (ptaEnabled) {
        // Signal old OFF callback when unrequesting
        // in case PTA is disabled whilst in the midst of a request,
        // so the requestor's state machine doesn't lock up.
        if ( (ptaReq == PTA_REQ_OFF)
           &&(txReq != PTA_REQ_OFF)
           &&(txCb != NULL)
           &&(txReq & PTA_REQCB_OFF) ) {
            (*txCb)(PTA_REQCB_OFF);
        }
        txCb  = ptaCb;
        if (txReq == ptaReq) {
          // Save a little time if redundant request
        } else {
          txReq = ptaReq;
          ptaUpdateReqIsr();
        }
        status = EMBER_SUCCESS;
      }
    )//ATOMIC
    return status;
  }

  EmberStatus halPtaSetRxRequest(halPtaReq_t ptaReq, halPtaCb_t ptaCb)
  {
    EmberStatus status = EMBER_ERR_FATAL;
    ATOMIC(
      if (ptaEnabled) {
        // Signal old OFF callback when unrequesting
        // in case PTA is disabled whilst in the midst of a request,
        // so the requestor's state machine doesn't lock up.
        if ( (ptaReq == PTA_REQ_OFF)
           &&(rxReq != PTA_REQ_OFF)
           &&(rxCb != NULL)
           &&(rxReq & PTA_REQCB_OFF) ) {
            (*rxCb)(PTA_REQCB_OFF);
        }
        rxCb  = ptaCb;
        if (rxReq == ptaReq) {
          // Save a little time if redundant request
        } else {
          rxReq = ptaReq;
          ptaUpdateReqIsr();
        }
        status = EMBER_SUCCESS;
      }
    )//ATOMIC
    return status;
  }

  // Certain radios may want to override this with their own
  WEAK(void emRadioEnablePta(bool enable))
  {
  }

  EmberStatus halPtaSetEnable(bool enabled)
  {
    if (enabled) {
      ptaReqGpioCfg();
      ptaPriGpioCfg();
      ptaGntGpioCfg();
    } else {
      // Shut any pending PTA operation down
      (void) halPtaSetRxRequest(PTA_REQ_OFF, NULL);
      (void) halPtaSetTxRequest(PTA_REQ_OFF, NULL);
    }
    ptaEnabled = enabled;
    // Inform the radio in case PTA requires radio state manipulations
    emRadioEnablePta(enabled);
    halInternalPtaOrRhoNotifyRadio(); //Notify Radio land of new config
    return EMBER_SUCCESS;
  }

  bool halPtaIsEnabled(void)
  {
    return ptaEnabled;
  }

#ifdef  RHO_GPIO // BOARD_HEADER supports Radio HoldOff

#ifdef  WAKE_ON_DFL_RHO_VAR // Only define this if needed per board header
uint8_t WAKE_ON_DFL_RHO_VAR = WAKE_ON_DFL_RHO;
#endif//WAKE_ON_DFL_RHO_VAR

#define RHO_ENABLED_MASK  0x01u // RHO is enabled
#define RHO_RADIO_ON_MASK 0x02u // Radio is on (not sleeping)
static uint8_t rhoState;
static bool rhoAsserted = !!RHO_ASSERTED;

bool halGetRadioHoldOff(void)
{
  return (!!(rhoState & RHO_ENABLED_MASK));
}

// Return active state of Radio HoldOff GPIO pin
static bool halInternalRhoPinIsActive(void)
{
  return ( (rhoState & RHO_ENABLED_MASK)
         &&(((bool)halGpioRead(RHO_GPIO)) == rhoAsserted) );
}

void RHO_ISR(void)
{
  if (rhoState & RHO_ENABLED_MASK) {
    // Ack interrupt before reading GPIO to avoid potential of missing int
    INT_MISS = RHO_MISS_BIT;
    INT_GPIOFLAG = RHO_FLAG_BIT; // acknowledge the interrupt
    // Notify Radio land of state change
    halInternalPtaOrRhoNotifyRadio();
  } else {
   #ifdef  RHO_ISR_FOR_DFL
    // Defer to default GPIO config's ISR
    extern void RHO_ISR_FOR_DFL(void);
    RHO_ISR_FOR_DFL(); // This ISR is expected to acknowledge the interrupt
   #else//!RHO_ISR_FOR_DFL
    INT_GPIOFLAG = RHO_FLAG_BIT; // acknowledge the interrupt
   #endif//RHO_ISR_FOR_DFL
  }
}

EmberStatus halSetRadioHoldOff(bool enabled)
{
  // If enabling afresh or disabling after having been enabled
  // restart from a fresh state just in case.
  // Otherwise don't touch a setup that might already have been
  // put into place by the default 'DFL' use (e.g. a button).
  // When disabling after having been enabled, it is up to the
  // board header caller to reinit the default 'DFL' use if needed.
  if (enabled || (rhoState & RHO_ENABLED_MASK)) {
    RHO_INTCFG = 0;              //disable RHO triggering
    INT_CFGCLR = RHO_INT_EN_BIT; //clear RHO top level int enable
    INT_GPIOFLAG = RHO_FLAG_BIT; //clear stale RHO interrupt
    INT_MISS = RHO_MISS_BIT;     //clear stale missed RHO interrupt
  }

  rhoState = (rhoState & ~RHO_ENABLED_MASK) | (enabled ? RHO_ENABLED_MASK : 0);

  // Reconfigure GPIOs for desired state
  ADJUST_GPIO_CONFIG_DFL_RHO(enabled);

  if (enabled) {
    // Here we sense asserted state is opposite of its current output state.
    rhoAsserted = !halGpioReadOutput(RHO_GPIO);
    // Only update radio if it's on, otherwise defer to when it gets turned on
    if (rhoState & RHO_RADIO_ON_MASK) {
      halInternalPtaOrRhoNotifyRadio(); //Notify Radio land of current state
      INT_CFGSET = RHO_INT_EN_BIT; //set top level interrupt enable
      // Interrupt on now, ISR will maintain proper state
    }
  } else {
    halInternalPtaOrRhoNotifyRadio(); //Notify Radio land of configured state
    // Leave interrupt state untouched (probably turned off above)
  }

  return EMBER_SUCCESS;
}

static void halStackRadioHoldOffPowerDown(void)
{
  rhoState &= ~RHO_RADIO_ON_MASK;
  if (rhoState & RHO_ENABLED_MASK) {
    // When sleeping radio, no need to monitor RHO anymore
    INT_CFGCLR = RHO_INT_EN_BIT; //clear RHO top level int enable
  }
}

static void halStackRadioHoldOffPowerUp(void)
{
  rhoState |= RHO_RADIO_ON_MASK;
  if (rhoState & RHO_ENABLED_MASK) {
    // When waking radio, set up initial state and resume monitoring
    INT_CFGCLR = RHO_INT_EN_BIT; //ensure RHO interrupt is off
    RHO_ISR(); // Manually call ISR to assess current state
    INT_CFGSET = RHO_INT_EN_BIT; //enable RHO interrupt
  }
}

#else//!RHO_GPIO

// Stubs in case someone insists on referencing them or for PTA w/o RHO

bool halGetRadioHoldOff(void)
{
  return false;
}

EmberStatus halSetRadioHoldOff(bool enabled)
{
  return (enabled ? EMBER_BAD_ARGUMENT : EMBER_SUCCESS);
}

static void halStackRadioHoldOffPowerDown(void)
{
}

static void halStackRadioHoldOffPowerUp(void)
{
  halInternalPtaOrRhoNotifyRadio();
}

#define halInternalRhoPinIsActive() (false)

#endif//RHO_GPIO // Board header supports Radio HoldOff

extern void emRadioHoldOffIsr(bool active);

static void halInternalPtaOrRhoNotifyRadio(void)
{
  bool ptaRho = ( ptaEnabled                       // PTA is enabled and:
                &&( (!ptaReqGpioOutAsserted())     // not REQUESTing or
                  ||(!ptaGntGpioInAsserted()) ) ); // REQUEST not GRANTed
  emRadioHoldOffIsr(ptaRho || halInternalRhoPinIsActive());
}
