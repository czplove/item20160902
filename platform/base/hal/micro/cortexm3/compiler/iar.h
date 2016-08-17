/** @file hal/micro/cortexm3/compiler/iar.h
 * See @ref iar for detailed documentation.
 *
 * <!-- Copyright 2008-2011 by Ember Corporation. All rights reserved.   *80*-->
 */

/** @addtogroup iar
 * @brief Compiler and Platform specific definitions and typedefs for the
 *  IAR ARM C compiler.
 *
 * @note iar.h should be included first in all source files by setting the
 *  preprocessor macro PLATFORM_HEADER to point to it.  iar.h automatically
 *  includes platform-common.h.
 *
 *  See iar.h and platform-common.h for source code.
 *@{
 */

#ifndef __IAR_H__
#define __IAR_H__

#ifndef __ICCARM__
  #error Improper PLATFORM_HEADER
#endif

#if (__VER__ < 6040002)
  #error Only IAR EWARM versions greater than 6.40.2 are supported
#endif // __VER__


#ifndef DOXYGEN_SHOULD_SKIP_THIS
  #include <intrinsics.h>
  #include <stdarg.h>
  #include <stdint.h>
  #include <stdbool.h>
  #include <string.h>
  #if defined (CORTEXM3_EMBER_MICRO)
    #include "micro/cortexm3/em35x/regs.h"
    #include "micro/cortexm3/micro-features.h"
  #elif defined (CORTEXM3_EFM32_MICRO)
    // EFR32, EZR32
    #include "em_device.h"  
    #include "micro/cortexm3/efm32/regs.h"
    #include "micro/cortexm3/micro-features.h"
    #define NVIC_CONFIG "hal/micro/cortexm3/efm32/nvic-config.h"
  #else
    #error Unknown CORTEXM3 micro
  #endif
  //Provide a default NVIC configuration file.  The build process can
  //override this if it needs to.
  #ifndef NVIC_CONFIG
    #define NVIC_CONFIG "hal/micro/cortexm3/nvic-config.h"
  #endif
//[[
#ifdef  EMBER_EMU_TEST
  #ifdef  I_AM_AN_EMULATOR
    // This register is defined for both the chip and the emulator with
    // with distinct reset values.  Need to undefine to avoid preprocessor
    // collision.
    #undef DATA_EMU_REGS_BASE
    #undef DATA_EMU_REGS_END
    #undef DATA_EMU_REGS_SIZE
    #undef I_AM_AN_EMULATOR
    #undef I_AM_AN_EMULATOR_REG
    #undef I_AM_AN_EMULATOR_ADDR
    #undef I_AM_AN_EMULATOR_RESET
    #undef I_AM_AN_EMULATOR_I_AM_AN_EMULATOR
    #undef I_AM_AN_EMULATOR_I_AM_AN_EMULATOR_MASK
    #undef I_AM_AN_EMULATOR_I_AM_AN_EMULATOR_BIT
    #undef I_AM_AN_EMULATOR_I_AM_AN_EMULATOR_BITS
  #endif//I_AM_AN_EMULATOR
  #if defined (CORTEXM3_EMBER_MICRO)
    #include "micro/cortexm3/em35x/regs-emu.h"
    #include "micro/cortexm3/micro-features.h"
  #else
    #error MICRO currently not supported for emulator builds.
  #endif
#endif//EMBER_EMU_TEST
//]]

// suppress warnings about unknown pragmas
//  (as they may be pragmas known to other platforms)
#pragma diag_suppress = pe161

#endif  // DOXYGEN_SHOULD_SKIP_THIS


/** \name Master Variable Types
 * These are a set of typedefs to make the size of all variable declarations
 * explicitly known.
 */
//@{
/**
 * @brief A typedef to make the size of the variable explicitly known.
 */
typedef bool boolean; /*To ease adoption of bool instead of boolean.*/
typedef unsigned char  int8u;
typedef signed   char  int8s;
typedef unsigned short int16u;
typedef signed   short int16s;
typedef unsigned int   int32u;
typedef signed   int   int32s;
typedef unsigned long long int64u;
typedef signed   long long int64s;
typedef unsigned int   PointerType;
//@} \\END MASTER VARIABLE TYPES

/**
 * @brief Denotes that this platform supports 64-bit data-types.
 */
#define HAL_HAS_INT64

/**
 * @brief Use the Master Program Memory Declarations from platform-common.h
 */
#define _HAL_USE_COMMON_PGM_



////////////////////////////////////////////////////////////////////////////////
/** \name Miscellaneous Macros
 */
////////////////////////////////////////////////////////////////////////////////
//@{

/**
 * @brief A convenient method for code to know what endiannes processor
 * it is running on.  For the Cortex-M3, we are little endian.
 */
#define BIGENDIAN_CPU  false


/**
 * @brief Define intrinsics for NTOHL and NTOHS to save code space by
 * making endian.c compile to nothing.
 */
#define NTOHS(val) (__REV16(val))
#define NTOHL(val) (__REV(val))


/**
 * @brief A friendlier name for the compiler's intrinsic for not
 * stripping.
 */
#define NO_STRIPPING  __root


/**
 * @brief A friendlier name for the compiler's intrinsic for eeprom
 * reference.
 */
#define EEPROM  errorerror


#ifndef __SOURCEFILE__
  /**
   * @brief The __SOURCEFILE__ macro is used by asserts to list the
   * filename if it isn't otherwise defined, set it to the compiler intrinsic
   * which specifies the whole filename and path of the sourcefile
   */
  #define __SOURCEFILE__ __FILE__
#endif


#undef assert
/**
 * @brief A prototype definition for use by the assert macro. (see
 * hal/micro/micro.h)
 */
void halInternalAssertFailed(const char *filename, int linenumber);

/**
 * @brief A custom implementation of the C language assert macro.
 * This macro implements the conditional evaluation and calls the function
 * halInternalAssertFailed(). (see hal/micro/micro.h)
 */
#ifdef DOXYGEN_SHOULD_SKIP_THIS
#define assert(condition)
#else //DOXYGEN_SHOULD_SKIP_THIS
// Don't define PUSH_REGS_BEFORE_ASSERT if it causes problems with the compiler.
// For example, in some compilers any inline assembly disables all optimization.
//
// For IAR V5.30, inline assembly apparently does not affect compiler output.
//#define PUSH_REGS_BEFORE_ASSERT
#ifdef PUSH_REGS_BEFORE_ASSERT
#define assert(condition) \
      do { if (! (condition)) { \
        asm("PUSH {R0,R1,R2,LR}"); \
        halInternalAssertFailed(__SOURCEFILE__, __LINE__); } } while(0)
#else
#define assert(condition) \
      do { if (! (condition)) { \
        halInternalAssertFailed(__SOURCEFILE__, __LINE__); } } while(0)
#endif
#endif //DOXYGEN_SHOULD_SKIP_THIS

/**
 * @brief Macro to reset the watchdog timer.  Note:  be very very
 * careful when using this as you can easily get into an infinite loop if you
 * are not careful.
 */
void halInternalResetWatchDog(void);
#ifdef RTOS
  void rtosResetWatchdog(void);
  #define halResetWatchdog()  rtosResetWatchdog()
#else
  #define halResetWatchdog()  halInternalResetWatchDog()
#endif //RTOS

/**
 * @brief Define __attribute__ to nothing since it isn't handled by IAR.
 */
#define __attribute__(...)


/**
 * @brief Declare a variable as unused to avoid a warning.  Has no effect
 * in IAR builds
 */
#define UNUSED

/**
 * @brief Some platforms need to cast enum values that have the high bit set.
 */
#define SIGNED_ENUM


/**
 * @brief Define the magic value that is interpreted by IAR C-SPY's Stack View.
 */
#define STACK_FILL_VALUE  0xCDCDCDCD

/**
 * @brief Define a generic RAM function identifier to a compiler specific one.
 */
#ifdef RAMEXE
  //If the whole build is running out of RAM, as chosen by the RAMEXE build
  //define, then define RAMFUNC to nothing since it's not needed.
  #define RAMFUNC
#else //RAMEXE
  #define RAMFUNC __ramfunc
#endif //RAMEXE

/**
 * @brief Define a generic no operation identifier to a compiler specific one.
 */
#define NO_OPERATION() __no_operation()

/**
 * @brief A convenience macro that makes it easy to change the field of a
 * register to any value.
 */
#define SET_REG_FIELD(reg, field, value)                      \
  do{                                                         \
    reg = ((reg & (~field##_MASK)) |                          \
           (uint32_t) (((uint32_t) value) << field##_BIT));       \
  }while(0)

/**
 * @brief Stub for code not running in simulation.
 */
#define simulatedTimePasses()
/**
 * @brief Stub for code not running in simulation.
 */
#define simulatedTimePassesMs(x)
/**
 * @brief Stub for code not running in simulation.
 */
#define simulatedSerialTimePasses()


/**
 * @brief Use the Divide and Modulus Operations from platform-common.h
 */
#define _HAL_USE_COMMON_DIVMOD_


/**
 * @brief Provide a portable way to specify the segment where a variable
 * lives.
 */
#define VAR_AT_SEGMENT(__variableDeclaration, __segmentName) \
    __variableDeclaration @ __segmentName

/**
 * @brief Convinience macro for turning a token into a string 
 */
#define STRINGIZE(X) #X
    
/**
 * @brief Provide a portable way to align data.
 */
#define ALIGNMENT(X) \
  _Pragma( STRINGIZE( data_alignment=X ) )

/**
 * @brief Provide a portable way to specify a symbol as weak
 */
#define WEAK(__symbol) \
    __weak __symbol

/**
 * @brief Provide a portable way to specify a non initialized symbol
 */
#define NO_INIT(__symbol) \
    __no_init __symbol

////////////////////////////////////////////////////////////////////////////////
//@}  // end of Miscellaneous Macros
////////////////////////////////////////////////////////////////////////////////

/** @name Portable segment names
 *@{
 */
/**
 * @brief Portable segment names
 */
#define __NO_INIT__       ".noinit"
#define __DEBUG_CHANNEL__ "DEBUG_CHANNEL"
#define __INTVEC__ ".intvec"
#define __CSTACK__ "CSTACK"
#define __RESETINFO__ "RESETINFO"
#define __DATA_INIT__ ".data_init"
#define __DATA__ ".data"
#define __BSS__ ".bss"
#define __APP_RAM__ "APP_RAM"
#define __CONST__ ".rodata"
#define __TEXT__ ".text"
#define __TEXTRW_INIT__ ".textrw_init"
#define __TEXTRW__ ".textrw"
#define __AAT__ "AAT"  // Application address table
#define __BAT__ "BAT"  // Bootloader address table
#define __BAT_INIT__ "BAT"  // Bootloader address table
#define __FAT__ "FAT"  // Fixed address table
#define __RAT__ "RAT"  // Ramexe address table
#define __NVM__ "NVM" //Non-Volatile Memory data storage
#define __SIMEE__ "SIMEE" //Simulated EEPROM storage
#define __PSSTORE__ "PSSTORE" //PS Store storage
#define __EMHEAP__ "EMHEAP" // Heap region for extra memory
#define __EMHEAP_OVERLAY__ "EMHEAP_overlay" // Heap and reset info combined
#define __GUARD_REGION__ "GUARD_REGION" // Guard page between heap and stack
#define __DLIB_PERTHREAD_INIT__ "__DLIB_PERTHREAD_init" // DLIB_PERTHREAD flash initialization data
#define __DLIB_PERTHREAD_INITIALIZED_DATA__ "DLIB_PERTHREAD_INITIALIZED_DATA" // DLIB_PERTHREAD RAM region to init
#define __DLIB_PERTHREAD_ZERO_DATA__ "DLIB_PERTHREAD_ZERO_DATA" // DLIB_PERTHREAD RAM region to zero out
#define __INTERNAL_STORAGE__ "INTERNAL_STORAGE" //Internal storage region
#define __UNRETAINED_RAM__ "UNRETAINED_RAM" //Region of RAM not retained during deepsleep


//=============================================================================
// The '#pragma segment=' declaration must be used before attempting to access
// the segments so the compiler properly handles the __segment_*() functions.
//
// The segment names used here are the default segment names used by IAR. Refer
// to the IAR Compiler Reference Guide for a proper description of these
// segments.
//=============================================================================
#pragma segment=__NO_INIT__
#pragma segment=__DEBUG_CHANNEL__
#pragma segment=__INTVEC__
#pragma segment=__CSTACK__
#pragma segment=__RESETINFO__
#pragma segment=__DATA_INIT__
#pragma segment=__DATA__
#pragma segment=__BSS__
#pragma segment=__APP_RAM__
#pragma segment=__CONST__
#pragma segment=__TEXT__
#pragma segment=__TEXTRW_INIT__
#pragma segment=__TEXTRW__
#pragma segment=__AAT__
#pragma segment=__BAT__
#pragma segment=__FAT__
#pragma segment=__RAT__
#pragma segment=__NVM__
#pragma segment=__SIMEE__
#pragma segment=__PSSTORE__
#pragma segment=__EMHEAP__
#pragma segment=__EMHEAP_OVERLAY__
#pragma segment=__GUARD_REGION__
#pragma segment=__DLIB_PERTHREAD_INIT__
#pragma segment=__DLIB_PERTHREAD_INITIALIZED_DATA__
#pragma segment=__DLIB_PERTHREAD_ZERO_DATA__
#pragma segment=__INTERNAL_STORAGE__
#pragma segment=__UNRETAINED_RAM__

#define _NO_INIT_SEGMENT_BEGIN                         __segment_begin(__NO_INIT__)
#define _DEBUG_CHANNEL_SEGMENT_BEGIN                   __segment_begin(__DEBUG_CHANNEL__)
#define _INTVEC_SEGMENT_BEGIN                          __segment_begin(__INTVEC__)
#define _CSTACK_SEGMENT_BEGIN                          __segment_begin(__CSTACK__)
#define _RESETINFO_SEGMENT_BEGIN                       __segment_begin(__RESETINFO__)
#define _DATA_INIT_SEGMENT_BEGIN                       __segment_begin(__DATA_INIT__)
#define _DATA_SEGMENT_BEGIN                            __segment_begin(__DATA__)
#define _BSS_SEGMENT_BEGIN                             __segment_begin(__BSS__)
#define _APP_RAM_SEGMENT_BEGIN                         __segment_begin(__APP_RAM__)
#define _CONST_SEGMENT_BEGIN                           __segment_begin(__CONST__)
#define _TEXT_SEGMENT_BEGIN                            __segment_begin(__TEXT__)
#define _TEXTRW_INIT_SEGMENT_BEGIN                     __segment_begin(__TEXTRW_INIT__)
#define _TEXTRW_SEGMENT_BEGIN                          __segment_begin(__TEXTRW__)
#define _AAT_SEGMENT_BEGIN                             __segment_begin(__AAT__)
#define _BAT_SEGMENT_BEGIN                             __segment_begin(__BAT__)
#define _BAT_INIT_SEGMENT_BEGIN                        __segment_begin(__BAT_INIT__)
#define _FAT_SEGMENT_BEGIN                             __segment_begin(__FAT__)
#define _RAT_SEGMENT_BEGIN                             __segment_begin(__RAT__)
#define _NVM_SEGMENT_BEGIN                             __segment_begin(__NVM__)
#define _SIMEE_SEGMENT_BEGIN                           __segment_begin(__SIMEE__)
#define _PSSTORE_SEGMENT_BEGIN                         __segment_begin(__PSSTORE__)
#define _EMHEAP_SEGMENT_BEGIN                          __segment_begin(__EMHEAP__)
#define _EMHEAP_OVERLAY_SEGMENT_BEGIN                  __segment_begin(__EMHEAP_OVERLAY__)
#define _GUARD_REGION_SEGMENT_BEGIN                    __segment_begin(__GUARD_REGION__)
#define _DLIB_PERTHREAD_INIT_SEGMENT_BEGIN             __segment_begin(__DLIB_PERTHREAD_INIT__)
#define _DLIB_PERTHREAD_INITIALIZED_DATA_SEGMENT_BEGIN __segment_begin(__DLIB_PERTHREAD_INITIALIZED_DATA__)
#define _DLIB_PERTHREAD_ZERO_DATA_SEGMENT_BEGIN        __segment_begin(__DLIB_PERTHREAD_ZERO_DATA__)
#define _INTERNAL_STORAGE_SEGMENT_BEGIN                __segment_begin(__INTERNAL_STORAGE__)
#define _UNRETAINED_RAM_SEGMENT_BEGIN                  __segment_begin(__UNRETAINED_RAM__)

#define _NO_INIT_SEGMENT_END                         __segment_end(__NO_INIT__)
#define _DEBUG_CHANNEL_SEGMENT_END                   __segment_end(__DEBUG_CHANNEL__)
#define _INTVEC_SEGMENT_END                          __segment_end(__INTVEC__)
#define _CSTACK_SEGMENT_END                          __segment_end(__CSTACK__)
#define _RESETINFO_SEGMENT_END                       __segment_end(__RESETINFO__)
#define _DATA_INIT_SEGMENT_END                       __segment_end(__DATA_INIT__)
#define _DATA_SEGMENT_END                            __segment_end(__DATA__)
#define _BSS_SEGMENT_END                             __segment_end(__BSS__)
#define _APP_RAM_SEGMENT_END                         __segment_end(__APP_RAM__)
#define _CONST_SEGMENT_END                           __segment_end(__CONST__)
#define _TEXT_SEGMENT_END                            __segment_end(__TEXT__)
#define _TEXTRW_INIT_SEGMENT_END                     __segment_end(__TEXTRW_INIT__)
#define _TEXTRW_SEGMENT_END                          __segment_end(__TEXTRW__)
#define _AAT_SEGMENT_END                             __segment_end(__AAT__)
#define _BAT_SEGMENT_END                             __segment_end(__BAT__)
#define _BAT_INIT_SEGMENT_END                        __segment_end(__BAT_INIT__)
#define _FAT_SEGMENT_END                             __segment_end(__FAT__)
#define _RAT_SEGMENT_END                             __segment_end(__RAT__)
#define _NVM_SEGMENT_END                             __segment_end(__NVM__)
#define _SIMEE_SEGMENT_END                           __segment_end(__SIMEE__)
#define _PSSTORE_SEGMENT_END                         __segment_end(__PSSTORE__)
#define _EMHEAP_SEGMENT_END                          __segment_end(__EMHEAP__)
#define _EMHEAP_OVERLAY_SEGMENT_END                  __segment_end(__EMHEAP_OVERLAY__)
#define _GUARD_REGION_SEGMENT_END                    __segment_end(__GUARD_REGION__)
#define _DLIB_PERTHREAD_INIT_SEGMENT_END             __segment_end(__DLIB_PERTHREAD_INIT__)
#define _DLIB_PERTHREAD_INITIALIZED_DATA_SEGMENT_END __segment_end(__DLIB_PERTHREAD_INITIALIZED_DATA__)
#define _DLIB_PERTHREAD_ZERO_DATA_SEGMENT_END        __segment_end(__DLIB_PERTHREAD_ZERO_DATA__)
#define _INTERNAL_STORAGE_SEGMENT_END                __segment_end(__INTERNAL_STORAGE__)
#define _UNRETAINED_RAM_SEGMENT_END                  __segment_end(__UNRETAINED_RAM__)

#define _NO_INIT_SEGMENT_SIZE                         __segment_size(__NO_INIT__)
#define _DEBUG_CHANNEL_SEGMENT_SIZE                   __segment_size(__DEBUG_CHANNEL__)
#define _INTVEC_SEGMENT_SIZE                          __segment_size(__INTVEC__)
#define _CSTACK_SEGMENT_SIZE                          __segment_size(__CSTACK__)
#define _RESETINFO_SEGMENT_SIZE                       __segment_size(__RESETINFO__)
#define _DATA_INIT_SEGMENT_SIZE                       __segment_size(__DATA_INIT__)
#define _DATA_SEGMENT_SIZE                            __segment_size(__DATA__)
#define _BSS_SEGMENT_SIZE                             __segment_size(__BSS__)
#define _APP_RAM_SEGMENT_SIZE                         __segment_size(__APP_RAM__)
#define _CONST_SEGMENT_SIZE                           __segment_size(__CONST__)
#define _TEXT_SEGMENT_SIZE                            __segment_size(__TEXT__)
#define _TEXTRW_INIT_SEGMENT_SIZE                     __segment_size(__TEXTRW_INIT__)
#define _TEXTRW_SEGMENT_SIZE                          __segment_size(__TEXTRW__)
#define _AAT_SEGMENT_SIZE                             __segment_size(__AAT__)
#define _BAT_SEGMENT_SIZE                             __segment_size(__BAT__)
#define _BAT_INIT_SEGMENT_SIZE                        __segment_size(__BAT_INIT__)
#define _FAT_SEGMENT_SIZE                             __segment_size(__FAT__)
#define _RAT_SEGMENT_SIZE                             __segment_size(__RAT__)
#define _NVM_SEGMENT_SIZE                             __segment_size(__NVM__)
#define _SIMEE_SEGMENT_SIZE                           __segment_size(__SIMEE__)
#define _PSSTORE_SEGMENT_SIZE                         __segment_size(__PSSTORE__)
#define _EMHEAP_SEGMENT_SIZE                          __segment_size(__EMHEAP__)
#define _EMHEAP_OVERLAY_SEGMENT_SIZE                  __segment_size(__EMHEAP_OVERLAY__)
#define _GUARD_REGION_SEGMENT_SIZE                    __segment_size(__GUARD_REGION__)
#define _DLIB_PERTHREAD_INIT_SEGMENT_SIZE             __segment_size(__DLIB_PERTHREAD_INIT__)
#define _DLIB_PERTHREAD_INITIALIZED_DATA_SEGMENT_SIZE __segment_size(__DLIB_PERTHREAD_INITIALIZED_DATA__)
#define _DLIB_PERTHREAD_ZERO_DATA_SEGMENT_SIZE        __segment_size(__DLIB_PERTHREAD_ZERO_DATA__)
#define _INTERNAL_STORAGE_SEGMENT_SIZE                __segment_size(__INTERNAL_STORAGE__)
#define _UNRETAINED_RAM_SEGMENT_SIZE                  __segment_size(__UNRETAINED_RAM__)
/**@} */


//A utility function for inserting barrier instructions.  These
//instructions should be used whenever the MPU is enabled or disabled so
//that all memory/instruction accesses can complete before the MPU changes
//state.  
void _executeBarrierInstructions(void);

////////////////////////////////////////////////////////////////////////////////
/** \name Global Interrupt Manipulation Macros
 *
 * \b Note: The special purpose BASEPRI register is used to enable and disable
 * interrupts while permitting faults.
 * When BASEPRI is set to 1 no interrupts can trigger. The configurable faults
 * (usage, memory management, and bus faults) can trigger if enabled as well as 
 * the always-enabled exceptions (reset, NMI and hard fault).
 * When BASEPRI is set to 0, it is disabled, so any interrupt can triggger if 
 * its priority is higher than the current priority.
 */
////////////////////////////////////////////////////////////////////////////////
//@{

#define ATOMIC_LITE(blah) ATOMIC(blah)
#define DECLARE_INTERRUPT_STATE_LITE DECLARE_INTERRUPT_STATE
#define DISABLE_INTERRUPTS_LITE() DISABLE_INTERRUPTS()
#define RESTORE_INTERRUPTS_LITE() RESTORE_INTERRUPTS()

#ifdef BOOTLOADER
  #ifndef DOXYGEN_SHOULD_SKIP_THIS
    // The bootloader does not use interrupts
    #define DECLARE_INTERRUPT_STATE
    #define DISABLE_INTERRUPTS() do { } while(0)
    #define RESTORE_INTERRUPTS() do { } while(0)
    #define INTERRUPTS_ON() do { } while(0)
    #define INTERRUPTS_OFF() do { } while(0)
    #define INTERRUPTS_ARE_OFF() (false)
    #define ATOMIC(blah) { blah }
    #define HANDLE_PENDING_INTERRUPTS() do { } while(0)
    #define SET_BASE_PRIORITY_LEVEL(basepri) do { } while(0)
  #endif  // DOXYGEN_SHOULD_SKIP_THIS
#else  // BOOTLOADER
  
  #ifndef DOXYGEN_SHOULD_SKIP_THIS
    // A series of macros for the diagnostics of the global interrupt state.
    // These macros either enable or disable the debugging code in the
    // Interrupt Manipulation Macros as well as define the two pins used for
    // indicating the entry and exit of critical sections.
    //UNCOMMENT the below #define to enable interrupt debugging.
    //#define INTERRUPT_DEBUGGING
    #ifdef INTERRUPT_DEBUGGING
      // Designed to use the breakout board LED (PC5)
      #define ATOMIC_DEBUG(x)  x
      #define I_PIN         5
      #define I_PORT        C
      #define I_CFG_HL      H
      // For the concatenation to work, we need to define the regs via their own macros:
      #define I_SET_REG(port)           GPIO_P ## port ## SET
      #define I_CLR_REG(port)           GPIO_P ## port ## CLR
      #define I_OUT_REG(port)           GPIO_P ## port ## OUT
      #define I_CFG_MSK(port, pin)      P ## port ## pin ## _CFG_MASK
      #define I_CFG_BIT(port, pin)      P ## port ## pin ## _CFG_BIT
      #define I_CFG_REG(port, hl)       GPIO_P ## port ## CFG ## hl 
      // Finally, the macros to actually manipulate the interrupt status IO
      #define I_OUT(port, pin, hl) \
        do { I_CFG_REG(port, hl) &= ~(I_CFG_MSK(port, pin));      \
             I_CFG_REG(port, hl) |= (GPIOCFG_OUT << I_CFG_BIT(port, pin)); \
        } while (0)
      #define I_SET(port, pin)   do { I_SET_REG(port) = (BIT(pin)); } while (0)
      #define I_CLR(port, pin)   do { I_CLR_REG(port) = (BIT(pin)); } while (0)
      #define I_STATE(port, pin)   ((I_OUT_REG(port) & BIT(pin)) == BIT(pin))
      #if !defined(RTOS)
        #define DECLARE_INTERRUPT_STATE uint8_t _emIsrState, _emIsrDbgIoState
      #else //!defined(RTOS)
        #define DECLARE_INTERRUPT_STATE uint8_t _emIsrDbgIoState
      #endif //!defined(RTOS)
    #else//!INTERRUPT_DEBUGGING
      #define ATOMIC_DEBUG(x)
      #if !defined(RTOS)
        /**
         * @brief This macro should be called in the local variable
         * declarations section of any function which calls DISABLE_INTERRUPTS()
         * or RESTORE_INTERRUPTS().
         */
        #define DECLARE_INTERRUPT_STATE uint8_t _emIsrState
      #else //!defined(RTOS)
        /**
         * @brief This macro should be called in the local variable
         * declarations section of any function which calls DISABLE_INTERRUPTS()
         * or RESTORE_INTERRUPTS().
         */
        #define DECLARE_INTERRUPT_STATE
      #endif //!defined(RTOS)

    #endif//INTERRUPT_DEBUGGING
    
    // Prototypes for the BASEPRI and PRIMASK access functions.  They are very
    // basic and instantiated in assembly code in the file spmr.s37 (since
    // there are no C functions that cause the compiler to emit code to access
    // the BASEPRI/PRIMASK). This will inhibit the core from taking interrupts
    // with a priority equal to or less than the BASEPRI value.
    // Note that the priority values used by these functions are 5 bits and 
    // right-aligned
#if !defined(__CORTEX_M) || (__CORTEX_M >= 0x03)
    // Only Cortex-M3 and higher have BASEPRI
    extern uint8_t _readBasePri(void);
    extern void _writeBasePri(uint8_t priority);

    // Prototypes for BASEPRI functions used to disable and enable interrupts
    // while still allowing enabled faults to trigger.
    extern void _enableBasePri(void);
    extern uint8_t _disableBasePri(void);
    extern bool _basePriIsDisabled(void);
#endif
    
    // Prototypes for setting and clearing PRIMASK for global interrupt
    // enable/disable.
    extern void _setPriMask(void);
    extern void _clearPriMask(void);
    extern uint32_t _getPriMask(void);

    #if defined(RTOS)
      // Prototypes for the RTOS critical functions we will need to do ATOMIC()
      uint8_t rtosEnterCritical(void);
      uint8_t rtosExitCritical(void);
    #endif //defined(RTOS)

  #endif  // DOXYGEN_SHOULD_SKIP_THIS

  //The core Global Interrupt Manipulation Macros start here.
  
  // When building for an RTOS be sure to use compatible interrupt routines
  #if defined(RTOS)
    /**
     * @brief Disable interrupts, saving the previous state so it can be
     * later restored with RESTORE_INTERRUPTS().
     * \note Do not fail to call RESTORE_INTERRUPTS().
     * \note It is safe to nest this call.
     */
    #define DISABLE_INTERRUPTS()                    \
      do {                                          \
        rtosEnterCritical();                        \
        ATOMIC_DEBUG(                               \
          _emIsrDbgIoState = I_STATE(I_PORT,I_PIN); \
          I_SET(I_PORT, I_PIN);                     \
        )                                           \
      } while(0)

    /** 
     * @brief Restore the global interrupt state previously saved by
     * DISABLE_INTERRUPTS()
     * \note Do not call without having first called DISABLE_INTERRUPTS()
     * to have saved the state.
     * \note It is safe to nest this call.
     */
    #define RESTORE_INTERRUPTS()        \
      do {                              \
          ATOMIC_DEBUG(                 \
          if(!_emIsrDbgIoState)         \
            I_CLR(I_PORT, I_PIN);       \
        )                               \
        rtosExitCritical();             \
      } while(0)  
  #else
    /**
     * @brief Disable interrupts, saving the previous state so it can be
     * later restored with RESTORE_INTERRUPTS().
     * \note Do not fail to call RESTORE_INTERRUPTS().
     * \note It is safe to nest this call.
     */
    #if defined(__CORTEX_M) && (__CORTEX_M == 0x00)
      // Cortex-M0+ does not have BASEPRI, use PRIMASK instead
      #define DISABLE_INTERRUPTS()                    \
        do {                                          \
          _emIsrState = _getPriMask();                \
          _setPriMask();                              \
          ATOMIC_DEBUG(                               \
            _emIsrDbgIoState = I_STATE(I_PORT,I_PIN); \
            I_SET(I_PORT, I_PIN);                     \
          )                                           \
        } while(0)
    #else
      #define DISABLE_INTERRUPTS()                    \
        do {                                          \
          _emIsrState = _disableBasePri();            \
          ATOMIC_DEBUG(                               \
            _emIsrDbgIoState = I_STATE(I_PORT,I_PIN); \
            I_SET(I_PORT, I_PIN);                     \
          )                                           \
        } while(0)
    #endif
    
    /** 
     * @brief Restore the global interrupt state previously saved by
     * DISABLE_INTERRUPTS()
     * \note Do not call without having first called DISABLE_INTERRUPTS()
     * to have saved the state.
     * \note It is safe to nest this call.
     */
    #if defined(__CORTEX_M) && (__CORTEX_M == 0x00)
      // Cortex-M0+ does not have BASEPRI, use PRIMASK instead
      #define RESTORE_INTERRUPTS()      \
        do {                            \
          ATOMIC_DEBUG(                 \
            if(!_emIsrDbgIoState)       \
              I_CLR(I_PORT, I_PIN);     \
          )                             \
          if (!_emIsrState) {           \
            _clearPriMask();            \
          }                             \
        } while(0)
    #else
      #define RESTORE_INTERRUPTS()      \
        do {                            \
          ATOMIC_DEBUG(                 \
            if(!_emIsrDbgIoState)       \
              I_CLR(I_PORT, I_PIN);     \
          )                             \
          _writeBasePri(_emIsrState);   \
        } while(0)
      #endif
   #endif // defined(RTOS)
  
  /**
   * @brief Enable global interrupts without regard to the current or
   * previous state.
   */
#if defined(__CORTEX_M) && (__CORTEX_M == 0x00)
   // Cortex-M0+ does not have BASEPRI, use PRIMASK instead
  #define INTERRUPTS_ON()               \
    do {                                \
      ATOMIC_DEBUG(                     \
        I_OUT(I_PORT, I_PIN, I_CFG_HL); \
        I_CLR(I_PORT, I_PIN);           \
      )                                 \
      _clearPriMask();                  \
    } while(0)
#else
  #define INTERRUPTS_ON()               \
    do {                                \
      ATOMIC_DEBUG(                     \
        I_OUT(I_PORT, I_PIN, I_CFG_HL); \
        I_CLR(I_PORT, I_PIN);           \
      )                                 \
      _enableBasePri();                 \
    } while(0)
#endif
  
  /**
   * @brief Disable global interrupts without regard to the current or
   * previous state.
   */
#if defined(__CORTEX_M) && (__CORTEX_M == 0x00)
   // Cortex-M0+ does not have BASEPRI, use PRIMASK instead
  #define INTERRUPTS_OFF()      \
    do {                        \
      _setPriMask();            \
      ATOMIC_DEBUG(             \
        I_SET(I_PORT, I_PIN);   \
      )                         \
    } while(0)
#else
  #define INTERRUPTS_OFF()      \
    do {                        \
      (void)_disableBasePri();  \
      ATOMIC_DEBUG(             \
        I_SET(I_PORT, I_PIN);   \
      )                         \
    } while(0)
#endif

  /**
   * @returns true if global interrupts are disabled.
   */
#if defined(__CORTEX_M) && (__CORTEX_M == 0x00)
  #define INTERRUPTS_ARE_OFF() ( _getPriMask() )
#else
  #define INTERRUPTS_ARE_OFF() ( _basePriIsDisabled() )
#endif

  /**
   * @returns true if global interrupt flag was enabled when 
   * ::DISABLE_INTERRUPTS() was called.
   */
  #define INTERRUPTS_WERE_ON() (_emIsrState == 0)
  
  /**
   * @brief A block of code may be made atomic by wrapping it with this
   * macro.  Something which is atomic cannot be interrupted by interrupts.
   */
  #define ATOMIC(blah)         \
  {                            \
    DECLARE_INTERRUPT_STATE;   \
    DISABLE_INTERRUPTS();      \
    { blah }                   \
    RESTORE_INTERRUPTS();      \
  }
  
  
  /**
   * @brief Allows any pending interrupts to be executed. Usually this
   * would be called at a safe point while interrupts are disabled (such as
   * within an ISR).
   * 
   * Takes no action if interrupts are already enabled.
   */
  #define HANDLE_PENDING_INTERRUPTS() \
    do {                              \
      if (INTERRUPTS_ARE_OFF()) {     \
        INTERRUPTS_ON();              \
        INTERRUPTS_OFF();             \
      }                               \
   } while (0)
  
  
  /**
   * @brief Sets the base priority mask (BASEPRI) to the value passed,
   * bit shifted up by PRIGROUP_POSITION+1.  This will inhibit the core from
   * taking all interrupts with a preemptive priority equal to or less than
   * the BASEPRI mask.  This macro is dependent on the value of
   * PRIGROUP_POSITION in nvic-config.h. Note that the value 0 disables the
   * the base priority mask.
   *
   * Refer to the "PRIGROUP" table in nvic-config.h to know the valid values
   * for this macro depending on the value of PRIGROUP_POSITION.  With respect
   * to the table, this macro can only take the preemptive priority group
   * numbers denoted by the parenthesis.
   */
#if defined(__CORTEX_M) && (__CORTEX_M == 0x00)
   // Cortex-M0+ does not have BASEPRI
   // TODO: Delete this to catch usages
  #define SET_BASE_PRIORITY_LEVEL(basepri) \
    do {                                   \
    } while(0)
#else
  #define SET_BASE_PRIORITY_LEVEL(basepri) \
    do {                                   \
      _writeBasePri(basepri);              \
    } while(0)
#endif

#endif // BOOTLOADER
////////////////////////////////////////////////////////////////////////////////
//@}  // end of Global Interrupt Manipulation Macros
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief If the line below is uncommented we will use Ember memory APIs,
 * otherwise, we will use the C Standard library (memset,memcpy,memmove) APIs.
 */
#define _HAL_USE_COMMON_MEMUTILS_

////////////////////////////////////////////////////////////////////////////////
/** \name External Declarations
 * These are routines that are defined in certain header files that we don't
 * want to include, e.g. stdlib.h
 */
////////////////////////////////////////////////////////////////////////////////
//@{

/**
 * @brief Returns the absolute value of I (also called the magnitude of I).
 * That is, if I is negative, the result is the opposite of I, but if I is
 * nonnegative the result is I.
 *
 * @param I  An integer.
 *
 * @return A nonnegative integer.
 */
int abs(int I);

////////////////////////////////////////////////////////////////////////////////
//@}  // end of External Declarations
////////////////////////////////////////////////////////////////////////////////


/**
 * @brief Include platform-common.h last to pick up defaults and common definitions.
 */
#define PLATCOMMONOKTOINCLUDE
  #include "hal/micro/generic/compiler/platform-common.h"
#undef PLATCOMMONOKTOINCLUDE

/**
 * @brief The kind of arguments the main function takes
 */
#define MAIN_FUNCTION_PARAMETERS void
#define MAIN_FUNCTION_ARGUMENTS 
#endif // __IAR_H__

/** @}  END addtogroup */

