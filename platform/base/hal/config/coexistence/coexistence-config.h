// Copyright 2016 Silicon Laboratories, Inc.

#if   defined(ZA_GENERATED_HEADER)
  #include ZA_GENERATED_HEADER  // AFV2
#elif defined(CONFIGURATION_HEADER)
  #include CONFIGURATION_HEADER // AFV6
#else
  #error Coexistence Configuration: cannot build without configuration header
#endif

// The coexistence interface has 4 signals.
//   RHO - radio hold off
//   REQ - request
//   GNT - grant
//   PRI - priority

// -----------------------------------------------------------------------------
// Defines

// These #define's are needed to enable the coexistence functionality at boot.

#define RADIO_HOLDOFF
#define ENABLE_PTA

// We need to define these letters so we can choose each signal's GPIO port
// based on the plugin option selected. They are #undef'd at the end of this
// header file.

#ifdef A
  #undef A
#endif
#define A 0

#ifdef B
  #undef B
#endif
#define B 1

#ifdef C
  #undef C
#endif
#define C 2

#ifdef D
  #undef D
#endif
#define D 3

#ifdef E
  #undef E
#endif
#define E 4

#ifdef F
  #undef F
#endif
#define F 5

#ifdef G
  #undef G
#endif
#define G 6

#ifdef H
  #undef H
#endif
#define H 7

// -----------------------------------------------------------------------------
// RHO

#if   (EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PORT == A)
  #define RHO_GPIO PORTA_PIN(EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PORT == B)
  #define RHO_GPIO PORTB_PIN(EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PORT == C)
  #define RHO_GPIO PORTC_PIN(EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PORT == D)
  #define RHO_GPIO PORTD_PIN(EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PORT == E)
  #define RHO_GPIO PORTE_PIN(EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PORT == F)
  #define RHO_GPIO PORTF_PIN(EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PORT == G)
  #define RHO_GPIO PORTG_PIN(EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PORT == H)
  #define RHO_GPIO PORTG_PIN(EMBER_AF_PLUGIN_COEXISTENCE_RHO_GPIO_PIN)
#else
  #error Coexistence Configuration: unknown RHO GPIO port
#endif

#ifdef EMBER_AF_PLUGIN_COEXISTENCE_RHO_ASSERTED
  #define RHO_ASSERTED 1
#else
  #define RHO_ASSERTED 0
#endif

// -----------------------------------------------------------------------------
// REQ

#if   (EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PORT == A)
  #define PTA_REQ_GPIO PORTA_PIN(EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PORT == B)
  #define PTA_REQ_GPIO PORTB_PIN(EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PORT == C)
  #define PTA_REQ_GPIO PORTC_PIN(EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PORT == D)
  #define PTA_REQ_GPIO PORTD_PIN(EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PORT == E)
  #define PTA_REQ_GPIO PORTE_PIN(EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PORT == F)
  #define PTA_REQ_GPIO PORTF_PIN(EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PORT == G)
  #define PTA_REQ_GPIO PORTG_PIN(EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PORT == H)
  #define PTA_REQ_GPIO PORTG_PIN(EMBER_AF_PLUGIN_COEXISTENCE_REQ_GPIO_PIN)
#else
  #error Coexistence Configuration: unknown REQ GPIO port
#endif

#ifdef EMBER_AF_PLUGIN_COEXISTENCE_REQ_ASSERTED
  #define PTA_REQ_ASSERTED 1
#else
  #define PTA_REQ_ASSERTED 0
#endif

#ifdef EMBER_AF_PLUGIN_COEXISTENCE_REQ_SHARED
  #define PTA_REQ_SHARED
#endif

#define PTA_REQ_MAX_BACKOFF_MASK EMBER_AF_PLUGIN_COEXISTENCE_REQ_MAX_BACKOFF_MASK

// -----------------------------------------------------------------------------
// GNT

#if   (EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PORT == A)
  #define PTA_GNT_GPIO PORTA_PIN(EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PORT == B)
  #define PTA_GNT_GPIO PORTB_PIN(EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PORT == C)
  #define PTA_GNT_GPIO PORTC_PIN(EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PORT == D)
  #define PTA_GNT_GPIO PORTD_PIN(EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PORT == E)
  #define PTA_GNT_GPIO PORTE_PIN(EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PORT == F)
  #define PTA_GNT_GPIO PORTF_PIN(EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PORT == G)
  #define PTA_GNT_GPIO PORTG_PIN(EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PORT == H)
  #define PTA_GNT_GPIO PORTG_PIN(EMBER_AF_PLUGIN_COEXISTENCE_GNT_GPIO_PIN)
#else
  #error Coexistence Configuration: unknown GNT GPIO port
#endif

#ifdef EMBER_AF_PLUGIN_COEXISTENCE_GNT_ASSERTED
  #define PTA_GNT_ASSERTED 1
#else
  #define PTA_GNT_ASSERTED 0
#endif

// -----------------------------------------------------------------------------
// PRI

#if   (EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PORT == A)
  #define PTA_PRI_GPIO PORTA_PIN(EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PORT == B)
  #define PTA_PRI_GPIO PORTB_PIN(EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PORT == C)
  #define PTA_PRI_GPIO PORTC_PIN(EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PORT == D)
  #define PTA_PRI_GPIO PORTD_PIN(EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PORT == E)
  #define PTA_PRI_GPIO PORTE_PIN(EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PORT == F)
  #define PTA_PRI_GPIO PORTF_PIN(EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PORT == G)
  #define PTA_PRI_GPIO PORTG_PIN(EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PIN)
#elif (EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PORT == H)
  #define PTA_PRI_GPIO PORTG_PIN(EMBER_AF_PLUGIN_COEXISTENCE_PRI_GPIO_PIN)
#else
  #error Coexistence Configuration: unknown PRI GPIO port
#endif

#ifdef EMBER_AF_PLUGIN_COEXISTENCE_PRI_ASSERTED
  #define PTA_PRI_ASSERTED 1
#else
  #define PTA_PRI_ASSERTED 0
#endif

// -----------------------------------------------------------------------------
// Cleanup

#if (RHO_GPIO == PTA_REQ_GPIO                             \
     || RHO_GPIO == PTA_GNT_GPIO                          \
     || RHO_GPIO == PTA_PRI_GPIO                          \
     || PTA_REQ_GPIO == PTA_GNT_GPIO                      \
     || PTA_REQ_GPIO == PTA_PRI_GPIO                      \
     || PTA_GNT_GPIO == PTA_PRI_GPIO)
  #error Coexistence Configuration: cannot select two of the same GPIO for signals
#endif

#undef A
#undef B
#undef C
#undef D
#undef E
#undef F
#undef G
#undef H
