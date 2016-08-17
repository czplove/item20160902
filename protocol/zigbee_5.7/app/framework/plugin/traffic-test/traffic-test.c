// Copyright 2016 Silicon Laboratories, Inc.                                *80*

#include "app/framework/include/af.h"
#include "app/framework/plugin/device-table/device-table.h"

#define TEST_COMPLETE_DELAY_MS 1000
#define READ_TEST_TIMEOUT_S    10
#define LEVEL_CONTROL_TEST_LEVEL_1  80
#define LEVEL_CONTROL_TEST_LEVEL_2  40

EmberEventControl emberAfPluginTrafficTestOnOffTestEventControl;
#define onOffTestEventControl emberAfPluginTrafficTestOnOffTestEventControl

EmberEventControl emberAfPluginTrafficTestLevelTestEventControl;
#define LevelTestEventControl emberAfPluginTrafficTestLevelTestEventControl

// default response tracking
static uint16_t numDefaultResponses;
static uint16_t numTxSendErrors;
static uint16_t numTotalMessages;
static uint16_t deviceEntry;
static uint16_t msBetweenToggles;
static uint16_t numMessagesRemaining;

//******************************************************************************
// This test command will rapidly send on and off commands at a controlled rate
// @param deviceEntry is the index into the device table of the device that
//        will be transmitting the on and off messages
// @param msBetweenToggles is the amount of time in Ms to wait before sending
//        the next on/off message
// @param numMessagesRemaining is the total number of messages to send
//******************************************************************************
void emberTrafficTestOnOffCommand(void)
{
  deviceEntry = (uint16_t)emberUnsignedCommandArgument(0);
  msBetweenToggles = (uint16_t)emberUnsignedCommandArgument(1);
  numMessagesRemaining = (uint16_t)emberUnsignedCommandArgument(2);

  // signal "stop test" with 0 mS.

  if(msBetweenToggles == 0) {
    emberEventControlSetInactive(onOffTestEventControl);
  } else {
    emberEventControlSetActive(onOffTestEventControl);
  }

  numDefaultResponses = 0;
  numTxSendErrors = 0;
  numTotalMessages = numMessagesRemaining;
}

//******************************************************************************
// This event handler will send another toggle message every msBetweenToggles
// milliseconds until all the messages have been sent, at which time it will
// print a diagnostic message and generate the test complete callback.
//******************************************************************************
void emberAfPluginTrafficTestOnOffTestEventHandler(void)
{
  if (numMessagesRemaining == 0) {
    // We were waiting for the last few transmissions.  Time to set the event
    // to inactive and print out the results.  
    emberEventControlSetInactive(onOffTestEventControl);
    emberSerialPrintf(APP_SERIAL, 
      "Default Responses:  %d, Send Errors %d, Total Messages %d\r\n",
      numDefaultResponses,
      numTxSendErrors,
      numTotalMessages);

    emberAfPluginTrafficTestReportResultsCallback(numDefaultResponses,
                                                   numTxSendErrors,
                                                   numTotalMessages);
    return;
  }

  // set up next event
  emberEventControlSetDelayMS(onOffTestEventControl, msBetweenToggles);

  emberAfFillCommandOnOffClusterToggle();
  emberAfPluginTrafficTestMessageBuiltCallback();

  deviceTableSend(deviceEntry);

  numMessagesRemaining--;

  // After the last message is sent, wait long enough to be sure that all of the
  // messages have gone out the radio
  if(numMessagesRemaining == 0) {
   emberEventControlSetDelayMS(onOffTestEventControl, TEST_COMPLETE_DELAY_MS);
  }
}


//******************************************************************************
// This test command will rapidly send moveToLevel commands at a controlled rate
//
// @param deviceEntry is the index into the device table of the device that
//        will be transmitting the on and off messages
// @param msBetweenToggles is the amount of time in Ms to wait before sending
//        the next message
// @param numMessagesRemaining is the total number of messages to send
//******************************************************************************
void emberTrafficTestLevelCommand(void)
{
  deviceEntry = (uint16_t)emberUnsignedCommandArgument(0);
  msBetweenToggles = (uint16_t)emberUnsignedCommandArgument(1);
  numMessagesRemaining = (uint16_t)emberUnsignedCommandArgument(2);

  // signal "stop test" with 0 mS.
  if(msBetweenToggles == 0) {
    emberEventControlSetInactive(LevelTestEventControl);
  } else {
    emberEventControlSetActive(LevelTestEventControl);
  }

  numDefaultResponses = 0;
  numTxSendErrors = 0;
  numTotalMessages = numMessagesRemaining;
}

//******************************************************************************
// This event handler will send another moveToLevel command every
// msBetweenToggles milliseconds until all the messages have been sent, at which
// time it will print a diagnostic message and generate the test complete
// callback.
//******************************************************************************
void emberAfPluginTrafficTestLevelTestEventHandler(void)
{
  if(numMessagesRemaining == 0) {
    // We were waiting for the last few transmissions.  Time to set the event
    // to inactive and print out the results.  
    emberEventControlSetInactive(LevelTestEventControl);

    emberSerialPrintf(APP_SERIAL,
      "Default Responses:  %d, Send Errors %d, Total Messages %d\r\n",
      numDefaultResponses,
      numTxSendErrors,
      numTotalMessages);

    emberAfPluginTrafficTestReportResultsCallback(numDefaultResponses,
                                                   numTxSendErrors,
                                                   numTotalMessages);

    return;
  }

  // set up next event
  emberEventControlSetDelayMS(LevelTestEventControl, msBetweenToggles);

  // Alternate between sending a move to 1/2 brightness and move to 1/4
  // brightness.  When constructing the moveToLevel command, byte 0 is 8 bit
  // brightness value, bytes 2:3 are the 16 bit transition time, which will be
  // set to zero for these tests.
  if((numMessagesRemaining % 2) == 0){
    emberAfFillCommandLevelControlClusterMoveToLevel(LEVEL_CONTROL_TEST_LEVEL_1,
                                                     0);
  } else {
    emberAfFillCommandLevelControlClusterMoveToLevel(LEVEL_CONTROL_TEST_LEVEL_2,
                                                     0);
  }

  deviceTableSend(deviceEntry);

  numMessagesRemaining--;
  if(numMessagesRemaining == 0) {
    emberEventControlSetDelayMS(LevelTestEventControl, TEST_COMPLETE_DELAY_MS);
  }
}

/** @brief Default Response
 *
 * This function is called by the application framework when a Default Response
 * command is received from an external device.  The application should return
 * TRUE if the message was processed or FALSE if it was not.
 *
 * @param clusterId The cluster identifier of this response.  Ver.: always
 * @param commandId The command identifier to which this is a response.  Ver.:
 * always
 * @param status Specifies either SUCCESS or the nature of the error that was
 * detected in the received command.  Ver.: always
 */
bool emberAfDefaultResponseCallback(EmberAfClusterId clusterId,
                                       uint8_t commandId,
                                       EmberAfStatus status)
{
  numDefaultResponses++;
  emberAfPluginTrafficTestDefaultResponseCallback(status,
                                                  clusterId,
                                                  commandId);

  return false;
}

// message error tracking:
void emAfTrafficTestTrackTxErrors(EmberStatus status)
{
  if(status != EMBER_SUCCESS)
    numTxSendErrors++;
}

