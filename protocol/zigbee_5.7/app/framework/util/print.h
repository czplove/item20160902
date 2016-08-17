// *******************************************************************
// * print.h
// *
// * Macros for functional area and per-cluster debug printing
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#ifndef __AF_DEBUG_PRINT__
#define __AF_DEBUG_PRINT__

#include "../include/af.h"

#if defined (EMBER_TEST) && !defined(EMBER_AF_PRINT_NAMES)
#include "debug-printing-test.h"
#endif

#if !defined(EMBER_AF_PRINT_OUTPUT) && defined(APP_SERIAL)
  #define EMBER_AF_PRINT_OUTPUT APP_SERIAL
#endif

extern uint16_t emberAfPrintActiveArea;


// These print functions are required by the CBKE crypto engine.
#define emberAfPrintZigbeeKey printZigbeeKey
#define emberAfPrintCert      printCert
#define emberAfPrintKey       printKey
#define emberAfPrintIeeeLine  printIeeeLine
#define emberAfPrintTextLine  printTextLine

#define emberAfPrintPublicKey(key)  printKey(true,  key)
#define emberAfPrintPrivateKey(key) printKey(false, key)

void emberAfPrintZigbeeKey(const uint8_t *key);
void emberAfPrintCert(const uint8_t *cert);
void emberAfPrintCert283k1(const uint8_t *cert);
void emberAfPrintKey(bool publicKey, const uint8_t *key);
void emberAfPrintKey283k1(bool publickKey, const uint8_t *key);
void emberAfPrintIeeeLine(const EmberEUI64 ieee);
void emberAfPrintTextLine(PGM_P text);

void emberAfPrint8ByteBlocks(uint8_t numBlocks,
                             const uint8_t *block,
                             bool crBetweenBlocks);
void emberAfPrintIssuer(const uint8_t *issuer);

void emberAfPrintChannelListFromMask(uint32_t channelMask);

#endif // __AF_DEBUG_PRINT__
