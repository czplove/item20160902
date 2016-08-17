// *****************************************************************************
// * trust-center.c
// *
// * Security code for a Trust Center node.
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *****************************************************************************

#include "app/framework/include/af.h"
#define USE_REAL_SECURITY_PROTOTYPES
#include "app/framework/security/af-security.h"
#include "app/framework/util/af-main.h"
#include "app/util/source-route-common.h"
#include "app/framework/util/attribute-storage.h"

#ifdef EMBER_AF_GENERATED_PLUGIN_TRUST_CENTER_JOIN_FUNCTION_DECLARATIONS
  EMBER_AF_GENERATED_PLUGIN_TRUST_CENTER_JOIN_FUNCTION_DECLARATIONS
#endif
//------------------------------------------------------------------------------
// Globals

#if defined(EMBER_AF_PRINT_SECURITY)
  // Corresponds to the EmberJoinDecision status codes
  static PGM_NO_CONST PGM_P joinDecisionText[] = {
    EMBER_JOIN_DECISION_STRINGS
  };

  // Corresponds to the EmberDeviceUpdate status codes
  static PGM_NO_CONST PGM_P deviceUpdateText[] = {
    EMBER_DEVICE_UPDATE_STRINGS
  };

#endif // EMBER_AF_PRINT_APP

static void initializeAllowTrustCenterRejoinGlobal(EmberKeyData *preconfiguredKey);

static EmberStatus permitRequestingTrustCenterLinkKey(void);
static EmberStatus permitRequestingApplicationLinkKey(void);
static EmberStatus setJoinPolicy(EmberJoinDecision decision);

bool emAfAllowTrustCenterRejoins = false;

//------------------------------------------------------------------------------

EmberStatus zaTrustCenterSetJoinPolicy(EmberJoinDecision decision)
{
  // Call the platform specific method to do this.
  return setJoinPolicy(decision);
}

EmberStatus zaTrustCenterSecurityPolicyInit(void)
{
  EmberStatus status = permitRequestingTrustCenterLinkKey();
  if (status == EMBER_SUCCESS) {
    status = permitRequestingApplicationLinkKey();
  }
  if (status != EMBER_SUCCESS) {
    return status;
  }

  // We always set the join policy to EMBER_USE_PRECONFIGURED_KEY
  // even in the case where the joining device doesn't have one.  This
  // is the safest thing to do.
  // In cases where devices don't have a preconfigured key, the TC must be
  // explicitly told to allow joining by calling the function below with
  // EMBER_SEND_KEY_IN_THE_CLEAR.  Once the device finishes joining the TC 
  // should set the policy back to EMBER_USE_PRECONFIGURED_KEY.
  return setJoinPolicy(EMBER_USE_PRECONFIGURED_KEY);
}

EmberStatus zaTrustCenterSecurityInit(bool centralizedNetwork)
{
  EmberInitialSecurityState state;
  EmberExtendedSecurityBitmask extended;
  EmberStatus status;
  const EmberAfSecurityProfileData *data = emAfGetCurrentSecurityProfileData();

  if (data == NULL) {
    return EMBER_ERR_FATAL;
  }

  MEMSET(&state, 0, sizeof(EmberInitialSecurityState));
  state.bitmask = data->tcBitmask;
  extended = data->tcExtendedBitmask;
  MEMMOVE(emberKeyContents(&state.preconfiguredKey),
          emberKeyContents(&data->preconfiguredKey),
          EMBER_ENCRYPTION_KEY_SIZE);

  // Random network key (highly recommended)
  status = emberAfGenerateRandomKey(&(state.networkKey));
  if (status != EMBER_SUCCESS) {
    return status;
  }

  // Check for distributed network.
  if (!centralizedNetwork)
    state.bitmask |= EMBER_DISTRIBUTED_TRUST_CENTER_MODE;

#if defined ZA_CLI_FULL
  // These functions will only be called if the full CLI is enabled and key 
  // values have been previously set on the CLI via the "changekey" command.
  getLinkKeyFromCli(&(state.preconfiguredKey));
  getNetworkKeyFromCli(&(state.networkKey));
#endif

  emberAfSecurityInitCallback(&state, &extended, true); // trust center?

  emberAfSecurityPrintln("set state to: 0x%2x", state.bitmask);
  status = emberSetInitialSecurityState(&state);
  if (status != EMBER_SUCCESS) {
    emberAfSecurityPrintln("security init TC: 0x%x", status);
    return status;
  }

  // Don't need to check on the status here, emberSetExtendedSecurityBitmask
  // always returns EMBER_SUCCESS.
  emberAfSecurityPrintln("set extended security to: 0x%2x", extended);
  emberSetExtendedSecurityBitmask(extended);

  initializeAllowTrustCenterRejoinGlobal(&state.preconfiguredKey);

  status = zaTrustCenterSecurityPolicyInit();
  if (status != EMBER_SUCCESS) {
    return status;
  }

  emAfClearLinkKeyTable();

  return EMBER_SUCCESS;
}

static void securityJoinNotify(EmberNodeId newNodeId,
                               EmberEUI64 newNodeEui64,
                               EmberNodeId parentOfNewNode,
                               EmberDeviceUpdate status,
                               EmberJoinDecision decision)
{

  // Allow users of the framework an opportunity to see this notification.
  emberAfTrustCenterJoinCallback(newNodeId, 
                                 newNodeEui64, 
                                 parentOfNewNode, 
                                 status, 
                                 decision);

#ifdef EMBER_AF_GENERATED_PLUGIN_TRUST_CENTER_JOIN_FUNCTION_CALLS
  EMBER_AF_GENERATED_PLUGIN_TRUST_CENTER_JOIN_FUNCTION_CALLS
#endif
  
#if defined(EMBER_AF_PLUGIN_CONCENTRATOR)
  if (parentOfNewNode != emberAfGetNodeId()) {
    sourceRouteAddEntryWithCloserNextHop(newNodeId, parentOfNewNode);
  }
#endif

#if defined(EMBER_AF_PRINT_SECURITY)
  emberAfSecurityPrintln("Trust Center Join Handler: status = %p, decision = %p (%x), shortid 0x%2x",
                 deviceUpdateText[status],
                 joinDecisionText[decision],
                 decision,
                 newNodeId);
  emberAfSecurityFlush();
#endif
}

static void initializeAllowTrustCenterRejoinGlobal(EmberKeyData *preconfiguredKey)
{
  // If we are using a global default link key (i.e., ZigBeeAlliance09 or
  // the SE Test dummy key (56777777...)), then we do not allow trust center
  // rejoins by default. We do not want to compromise the secrecy of the
  // network key if a device performs a trust center rejoin.

  EmberKeyData haKey = ZIGBEE_PROFILE_INTEROPERABILITY_LINK_KEY;
  EmberKeyData seTestKey = SE_SECURITY_TEST_LINK_KEY;

  emAfAllowTrustCenterRejoins
    = ((MEMCOMPARE(emberKeyContents(preconfiguredKey),
                   emberKeyContents(&haKey),
                   EMBER_ENCRYPTION_KEY_SIZE)
        == 0)
       && (MEMCOMPARE(emberKeyContents(preconfiguredKey),
                      emberKeyContents(&seTestKey),
                      EMBER_ENCRYPTION_KEY_SIZE)
           == 0));
}

//==============================================================================
// Platform Dependent Implementations

#if defined (EZSP_HOST)

static PGM_NO_CONST PGM_P ezspJoinPolicyText[] = {
  "Allow Joins",
  "Allow preconfigured key joins",
  "Allow rejoins only",
  "Disallow all joins and rejoins",
  "Allow Joins, rejoins have link key",
  "Allow Joins, ignore preconfigured key rejoins",
};

void ezspTrustCenterJoinHandler(EmberNodeId newNodeId,
                                EmberEUI64 newNodeEui64,
                                EmberDeviceUpdate status,
                                EmberJoinDecision policyDecision,
                                EmberNodeId parentOfNewNode)
{
  securityJoinNotify(newNodeId,
                     newNodeEui64,
                     parentOfNewNode,
                     status,
                     policyDecision);
}

static EmberStatus setJoinPolicy(EmberJoinDecision decision)
{
  EzspDecisionId id;
  if (decision == EMBER_USE_PRECONFIGURED_KEY) {
    if (emAfAllowTrustCenterRejoins) {
      id = EZSP_ALLOW_PRECONFIGURED_KEY_JOINS;
    } else {
      id = EZSP_IGNORE_TRUST_CENTER_REJOINS;
    }
  } else if (decision == EMBER_SEND_KEY_IN_THE_CLEAR) {
    id = EZSP_ALLOW_JOINS_REJOINS_HAVE_LINK_KEY;
  } else {  // EMBER_DENY_JOIN or EMBER_NO_ACTION
    id = EZSP_DISALLOW_ALL_JOINS_AND_REJOINS;
  }
  return emberAfSetEzspPolicy(EZSP_TRUST_CENTER_POLICY,
                              id,
                              "Trust Center Policy",
                              ezspJoinPolicyText[id]);
}

static EmberStatus permitRequestingTrustCenterLinkKey(void)
{
  const EmberAfSecurityProfileData *data = emAfGetCurrentSecurityProfileData();
  if (data == NULL) {
    return EMBER_ERR_FATAL;
  }

  return emberAfSetEzspPolicy(EZSP_TC_KEY_REQUEST_POLICY,
                              data->tcLinkKeyRequestPolicy,
                              "TC Key Request",
                              (data->tcLinkKeyRequestPolicy
                               ==  EMBER_AF_ALLOW_TC_KEY_REQUESTS
                               ? "Allow"
                               : "Deny"));
}

static EmberStatus permitRequestingApplicationLinkKey(void)
{
  const EmberAfSecurityProfileData *data = emAfGetCurrentSecurityProfileData();
  if (data == NULL) {
    return EMBER_ERR_FATAL;
  }
  return emberAfSetEzspPolicy(EZSP_APP_KEY_REQUEST_POLICY,
                              data->appLinkKeyRequestPolicy,
                              "App. Key Request",
                              (data->appLinkKeyRequestPolicy
                               == EMBER_AF_ALLOW_APP_KEY_REQUESTS
                               ? "Allow"
                               : "Deny"));
}

//------------------------------------------------------------------------------
// System-on-a-chip

#else 

static EmberJoinDecision defaultDecision = EMBER_USE_PRECONFIGURED_KEY;

EmberJoinDecision emberTrustCenterJoinHandler(EmberNodeId newNodeId,
                                              EmberEUI64 newNodeEui64,
                                              EmberDeviceUpdate status,
                                              EmberNodeId parentOfNewNode)
{
  EmberJoinDecision joinDecision = defaultDecision;

  if (status == EMBER_STANDARD_SECURITY_SECURED_REJOIN
      || status == EMBER_DEVICE_LEFT
      || status == EMBER_HIGH_SECURITY_SECURED_REJOIN
      || (!emAfAllowTrustCenterRejoins
          && status == EMBER_STANDARD_SECURITY_UNSECURED_REJOIN)) {
    joinDecision = EMBER_NO_ACTION;
  }

  securityJoinNotify(newNodeId,
                     newNodeEui64,
                     parentOfNewNode,
                     status,
                     joinDecision);

  return joinDecision;
}

static EmberStatus setJoinPolicy(EmberJoinDecision decision)
{
  defaultDecision = decision;
  return EMBER_SUCCESS;
}

static EmberStatus permitRequestingTrustCenterLinkKey(void)
{
  const EmberAfSecurityProfileData *data = emAfGetCurrentSecurityProfileData();
  if (data == NULL) {
    return EMBER_ERR_FATAL;
  }
  emberTrustCenterLinkKeyRequestPolicy = data->tcLinkKeyRequestPolicy;

  return EMBER_SUCCESS;
}

static EmberStatus permitRequestingApplicationLinkKey(void)
{
  const EmberAfSecurityProfileData *data = emAfGetCurrentSecurityProfileData();
  if (data == NULL) {
    return EMBER_ERR_FATAL;
  }
  emberAppLinkKeyRequestPolicy = data->appLinkKeyRequestPolicy;
  return EMBER_SUCCESS;
}

#endif // EZSP_HOST
