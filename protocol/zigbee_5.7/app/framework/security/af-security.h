// *******************************************************************
// * af-security.h
// *
// * Header file for App. Framework security code 
// *
// * Copyright 2009 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

void getLinkKeyFromCli(EmberKeyData* returnData);
void getNetworkKeyFromCli(EmberKeyData* returnData);

// If this flag is true we clear the link key table before forming or joining.
// If false, we skip clearing the link key once and we set this flag back to
// true.
extern bool emberAfClearLinkKeyTableUponFormingOrJoining;

extern const EmberAfSecurityProfileData emAfSecurityProfileData[];
const EmberAfSecurityProfileData *emAfGetCurrentSecurityProfileData(void);

void emAfClearLinkKeyTable(void);

extern bool emAfAllowTrustCenterRejoins;

#if defined(EMBER_AF_HAS_SECURITY_PROFILE_NONE) \
    || defined(EMBER_AF_HAS_SECURITY_PROFILE_Z3)
  // For no security, simply #define the security init routines to no-ops.
  // For ZigBee 3.0 security, plugins handle the initialization of security.
  #ifndef USE_REAL_SECURITY_PROTOTYPES
    #define zaNodeSecurityInit(centralizedNetwork)
    #define zaTrustCenterSecurityInit(centralizedNetwork)
    #define zaTrustCenterSetJoinPolicy(policy)
    #define zaTrustCenterSecurityPolicyInit()
  #endif
#else // All other security profiles.
  EmberStatus zaNodeSecurityInit(bool centralizedNetwork);
  EmberStatus zaTrustCenterSecurityInit(bool centralizedNetwork);
  EmberStatus zaTrustCenterSetJoinPolicy(EmberJoinDecision decision);
  EmberStatus zaTrustCenterSecurityPolicyInit(void);
#endif
