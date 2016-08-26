//
// network-creator-test.c
//
// Author: Andrew Keesler
//
// Created August 19, 2014
// Revamped April 17, 2015
//
// Unit test for network-creator state machine.
//

#include "app/framework/include/af.h"
#include "app/framework/test/script/afv2-scripted.h"

#include "network-creator.h"
#include "../scan-dispatch/scan-dispatch.h"

// -----------------------------------------------------------------------------
// GLOBALS.

#define PAN_ID 0x1234

#define RSSI (-30)
#define LQI  (80)

#ifndef EMBER_AF_PLUGIN_NETWORK_CREATOR_SCAN_DURATION
  #define EMBER_AF_PLUGIN_NETWORK_CREATOR_SCAN_DURATION (4)
#endif

// Security type: centralized
//                distributed
#define SECURITY_COUNT (2)
  #define CENTRALIZED (0)
  #define DISTRIBUTED (1)
static uint8_t securityTypes[] = { CENTRALIZED, DISTRIBUTED };
static uint8_t securityType;

// Channel mask: primary
//               secondary
#define CHANNEL_MASK_COUNT (2)
static uint32_t *channelMaskLocations[] = {
  &emAfPluginNetworkCreatorPrimaryChannelMask,
  &emAfPluginNetworkCreatorSecondaryChannelMask,
};
static uint32_t *currentChannelMask;

uint8_t emAfExtendedPanId[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,};

// -----------------------------------------------------------------------------
// COMPLETE CHECK.

void emberAfPluginNetworkCreatorCompleteCallback(const EmberNetworkParameters *params,
                                                 bool goToSecondaryMask)
{
  functionCallCheck("emberAfPluginNetworkCreatorCompleteCallback",
                    "i",
                    goToSecondaryMask);
}

#define addCompleteCheck(goToSecondaryMask)                     \
  addSimpleCheck("emberAfPluginNetworkCreatorCompleteCallback", \
                 "i",                                           \
                 (goToSecondaryMask))
  
// -----------------------------------------------------------------------------
// FORM CHECK.

EmberStatus emberFormNetwork(EmberNetworkParameters *parameters)
{
  long *contents = functionCallCheck("emberFormNetwork",
                                     "i",
                                     parameters->panId);

  assert(memcmp(parameters->extendedPanId,
                emAfExtendedPanId,
                EXTENDED_PAN_ID_SIZE)
         != 0);

  return (EmberStatus)contents[1];
}

#define addFormNetworkCheck(status, panId)                   \
  addSimpleCheck("emberFormNetwork",                         \
                 "i!",                                       \
                 (panId),                                    \
                 (status))

// -----------------------------------------------------------------------------
// NETWORK CREATOR SECURITY START CHECK.

EmberStatus emberAfPluginNetworkCreatorSecurityStart(bool centralizedSecurity)
{
  EmberStatus returnStatus;
  long *contents
    = functionCallCheck("emberAfPluginNetworkCreatorSecurityStart",
                        "i",
                        centralizedSecurity);

  returnStatus = (EmberStatus)contents[1];
  
  return returnStatus;
}

#define addNetworkCreatorSecurityStartCheck(centralizedSecurity, status) \
  addSimpleCheck("emberAfPluginNetworkCreatorSecurityStart",             \
                 "i!i",                                                  \
                 (centralizedSecurity),                                  \
                 (status))

// -----------------------------------------------------------------------------
// GET PAN ID CHECK.

EmberPanId emberAfPluginNetworkCreatorGetPanIdCallback(void)
{
  long *contents
    = functionCallCheck("emberAfPluginNetworkCreatorGetPanIdCallback",
                        "");

  return (EmberPanId)contents[0];
  return 0x1234;
}

#define addGetPanIdCheck(panId)                                   \
  addSimpleCheck("emberAfPluginNetworkCreatorGetPanIdCallback",   \
                 "!i",                                            \
                 (panId))

// -----------------------------------------------------------------------------
// SCAN HANDLER ACTION.

static void scanHandlerActionPrinter(Action *action)
{
  EmberNetworkScanType scanType = (EmberNetworkScanType)action->contents[0];
  fprintf(stderr, " received %s scan results:",
          (scanType == EMBER_ACTIVE_SCAN ? "active" : "energy"));
}

static void scanHandlerActionPerformer(Action *action)
{
  uint8_t statusOrRssi = (uint8_t)action->contents[0];
  uint8_t channelOrLqi = (uint8_t)action->contents[1];
  EmberNetworkScanType scanType = (EmberNetworkScanType)action->contents[2];
  bool isComplete = (bool)action->contents[3];
  bool isFailure = (bool)action->contents[4];

  uint8_t reallyChannel = (uint8_t)action->contents[5];
  EmberZigbeeNetwork network = { .channel = reallyChannel, };

  EmberAfPluginScanDispatchScanResults results = {
    { .status = statusOrRssi },
    { .channel = channelOrLqi },
    .network = &network,
    .mask = (scanType
             | (isComplete
                ? EM_AF_PLUGIN_SCAN_DISPATCH_SCAN_RESULTS_MASK_COMPLETE
                : 0)
             | (isFailure
                ? EM_AF_PLUGIN_SCAN_DISPATCH_SCAN_RESULTS_MASK_FAILURE
                : 0)),
  };

  // From network-creator.c.
  extern void scanHandler(EmberAfPluginScanDispatchScanResults *results);
  scanHandler(&results);
}

static ActionType scanHandlerAction = {
  "scanHandlerAction",
  "iiiiii", // statusOrRssi,channelOrLqi,scanType,complete,failure,reallyChannel
  scanHandlerActionPrinter,
  scanHandlerActionPerformer,
};

#define addScanHandlerAction(statusOrRssi,                              \
                             channelOrLqi,                              \
                             scanType,                                  \
                             complete,                                  \
                             failure,                                   \
                             reallyChannel)                             \
  addAction(&scanHandlerAction,                                         \
            (statusOrRssi),                                             \
            (channelOrLqi),                                             \
            (scanType),                                                 \
            (complete),                                                 \
            (failure),                                                  \
            (reallyChannel))

// -----------------------------------------------------------------------------
// SCHEDULE CHECK.

EmberStatus emberAfPluginScanDispatchScheduleScan(EmberAfPluginScanDispatchScanData *data)
{
  long *contents = functionCallCheck("emberAfPluginScanDispatchScheduleScan",
                                     "iii",
                                     data->scanType,
                                     data->channelMask,
                                     data->duration);
                    
  return (EmberStatus)contents[3];
}

#define addScheduleScanCheck(scanType, channelMask, duration, status) \
  addSimpleCheck("emberAfPluginScanDispatchScheduleScan",             \
                 "iii!i",                                             \
                 (scanType),                                          \
                 (channelMask),                                       \
                 (duration),                                          \
                 (status))

// -----------------------------------------------------------------------------
// START ACTION.

void startActionPrinter(Action *action)
{
  bool centralized = (bool)action->contents[0];
  EmberStatus expectedStatus = (EmberStatus)action->contents[1];
  fprintf(stderr, " starting network creator (%s) and expecting status 0x%02X",
          (centralized ? "centralized" : "distributed"),
          expectedStatus);
}

void startActionPerformer(Action *action)
{
  bool centralized = (bool)action->contents[0];
  EmberStatus expectedStatus = (EmberStatus)action->contents[1];
  scriptAssert(action,
               emberAfPluginNetworkCreatorStart(centralized) == expectedStatus);
}

static ActionType startAction = {
  "startAction",
  "ii", // centralized, status
  startActionPrinter,
  startActionPerformer,
};

#define addStartAction(centralized, status) \
  addAction(&startAction,                   \
            (centralized),                  \
            (status))

// -----------------------------------------------------------------------------
// FIND SOME NETWORKS.
            
static void findSomeNetworks(uint8_t networksFound,
                             bool goToSecondaryMask)
{
  uint8_t i, j, channel, network, channelMaskCount;
  bool shouldForm;

  testFrameworkNetworkState = EMBER_NO_NETWORK;

  channelMaskCount = CHANNEL_MASK_COUNT;
  if (!goToSecondaryMask) channelMaskCount --;

  for (i = 0; i < SECURITY_COUNT; i ++) {
    securityType = securityTypes[i];

    // Successfully start the network-creator state machine.
    addStartAction((securityType == CENTRALIZED ? true : false),
                   EMBER_SUCCESS);

    for (j = 0; j < channelMaskCount; j ++) {
      currentChannelMask = channelMaskLocations[j];

      // First we want an active scan...
      addScheduleScanCheck(EMBER_ACTIVE_SCAN,
                           *currentChannelMask,
                           EMBER_AF_PLUGIN_NETWORK_CREATOR_SCAN_DURATION,
                           EMBER_SUCCESS);
      // ...then we want an energy scan.
      addScheduleScanCheck(EMBER_ENERGY_SCAN,
                           *currentChannelMask,
                           EMBER_AF_PLUGIN_NETWORK_CREATOR_SCAN_DURATION,
                           EMBER_SUCCESS);
    
      // For each channel while we are still finding networks...
      network = networksFound;
      for (channel = EMBER_MIN_802_15_4_CHANNEL_NUMBER;
           (channel <= EMBER_MAX_802_15_4_CHANNEL_NUMBER) && network;
           channel ++) {
        // ...in our current channel mask...
        if (!READBIT(*currentChannelMask, channel)) continue;
        
        // ...we should first get the active scan results back...
        network --;
        addScanHandlerAction(RSSI,
                             LQI,
                             EMBER_ACTIVE_SCAN,
                             false, // complete?
                             false, // failure?
                             channel);
      }
      
      // ...and then we get a complete call for the active scan.
      addScanHandlerAction(EMBER_SUCCESS,
                           0, // scan complete,
                           EMBER_ACTIVE_SCAN,
                           true,  // complete?
                           false, // failure?
                           0); // channel doesn't matter
      
      // For each channel...
      for (channel = EMBER_MIN_802_15_4_CHANNEL_NUMBER;
           channel <= EMBER_MAX_802_15_4_CHANNEL_NUMBER;
           channel ++) {
        // ...that is left in our current channel mask...
        if (!READBIT(*currentChannelMask, channel)) continue;
        
        // ...we should first get the energy scan results back...
        addScanHandlerAction(RSSI,
                             LQI,
                             EMBER_ACTIVE_SCAN,
                             false, // complete?
                             false, // failure?
                             channel);
      }
      
      // ...and then we should get that the energy scan finished.
      addScanHandlerAction(EMBER_SUCCESS,
                           0, // scan complete
                           EMBER_ENERGY_SCAN,
                           true,  // complete?
                           false, // failure?
                           0); // channel doesn't matter
      
      // Make sure we ask the application about the PAN ID.
      addGetPanIdCheck(PAN_ID);

      // Whether or not we should form a network. For the sake of the
      // unit test for the secondary channel masks.
      shouldForm = (!goToSecondaryMask
                    || (currentChannelMask
                        == &emAfPluginNetworkCreatorSecondaryChannelMask));

      // We should set up security.
      addNetworkCreatorSecurityStartCheck(securityType == CENTRALIZED,
                                          EMBER_SUCCESS);

      // For each channel...
      for (channel = EMBER_MIN_802_15_4_CHANNEL_NUMBER;
           channel <= EMBER_MAX_802_15_4_CHANNEL_NUMBER;
           channel ++) {
        // ...that is still in our current channel mask...
        if (!READBIT(*currentChannelMask, channel)) continue;

        // We try to form a network.
        addFormNetworkCheck((shouldForm ? EMBER_SUCCESS : EMBER_ERR_FATAL), PAN_ID);

        if (shouldForm) break;
      }
      
      // If we were only supposed to use the primary mask, we are complete.
      // Else, we move to the secondary mask.
      if (shouldForm) addCompleteCheck(goToSecondaryMask);
      
    } // for currentChannelMask in channelMaskLocations
  } // for securityType in securityTypes

  runScript();
}

// -----------------------------------------------------------------------------
// TESTS.

static void alreadyOnNetworkTest(void)
{
  uint8_t i;

  testFrameworkNetworkState = EMBER_JOINED_NETWORK;

  // If we are already on a network, don't even run the state machine.
  for (i = 0; i < SECURITY_COUNT; i ++) {
    securityType = securityTypes[i];
    
    addStartAction((securityType == CENTRALIZED ? true : false),
                   EMBER_INVALID_CALL);
  }

  runScript();
}

static void noOtherNetworksAroundTest(void)     { findSomeNetworks(0, false); }
static void oneOtherNetworkAroundTest(void)     { findSomeNetworks(1, false); }
static void aLotOfOtherNetworksAroundTest(void) { findSomeNetworks(5, false); }
static void secondaryChannelMaskTest(void)      { findSomeNetworks(5, true);  }

// -----------------------------------------------------------------------------
// MAIN.

static Test tests[] = {
  { "already-on-network-test"            , alreadyOnNetworkTest          },
  { "no-other-networks-around-test"      , noOtherNetworksAroundTest     },
  { "one-other-network-around-test"      , oneOtherNetworkAroundTest     },
  { "a-lot-of-other-networks-around-test", aLotOfOtherNetworksAroundTest },
  { "secondary-channel-mask-test"        , secondaryChannelMaskTest      },
  { NULL                                 , NULL                      },
};

int main(int argc, char *argv[])
{
  Thunk test = parseTestArgument(argc, argv, tests);
  test();
  fprintf(stderr, " done ]\n");
  return 0;
}

// -----------------------------------------------------------------------------
// STUBS.

EmberStatus emberGenerateRandomKey(EmberKeyData *a)
{
  return EMBER_SUCCESS;
}

EmberStatus emberSetExtendedSecurityBitmask(EmberExtendedSecurityBitmask a)
{
  return EMBER_SUCCESS;
}

EmberStatus emberSetInitialSecurityState(EmberInitialSecurityState *a)
{
  return EMBER_SUCCESS;
}

// -----------------------------------------------------------------------------
// CALLBACKS.

void scriptTickCallback(void) {}
