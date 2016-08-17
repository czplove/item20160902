// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "han-message-dispatch.h"

#define EMBER_AF_HAN_MESSAGE_DISPATCH_SEND_LENGTH (255)

static uint8_t messagePayload[EMBER_AF_HAN_MESSAGE_DISPATCH_SEND_LENGTH];

// Prototypes
void emAfPluginHanMessageDispatchCliSend(void);

// Functions
void emAfPluginHanMessageDispatchCliSend(void)
{
  EmberEUI64 dst;
  int8u length;
  emberCopyEui64Argument(0, dst);
  length = emberCopyStringArgument(1,
                                   messagePayload,
                                   EMBER_AF_HAN_MESSAGE_DISPATCH_SEND_LENGTH,
                                   false);
  emberAfPluginHanMessageDispatchSend(dst, length, messagePayload);
}

