// This file is generated by Ember Desktop.  Please do not edit manually.
//
//

// Enclosing macro to prevent multiple inclusion
#ifndef __EMBER_AF_COMMAND_PARSE_HEADER__
#define __EMBER_AF_COMMAND_PARSE_HEADER__


// This is a set of generated prototype for functions that parse the
// the incomming message, and call appropriate command handler.

// Cluster: Identify, client
EmberAfStatus emberAfIdentifyClusterClientCommandParse(EmberAfClusterCommand *cmd);

// Cluster: Identify, server
EmberAfStatus emberAfIdentifyClusterServerCommandParse(EmberAfClusterCommand *cmd);

// Cluster: On/off, server
EmberAfStatus emberAfOnOffClusterServerCommandParse(EmberAfClusterCommand *cmd);

// Cluster: Level Control, server
EmberAfStatus emberAfLevelControlClusterServerCommandParse(EmberAfClusterCommand *cmd);

// Cluster: Poll Control, client
EmberAfStatus emberAfPollControlClusterClientCommandParse(EmberAfClusterCommand *cmd);

// Cluster: Color Control, server
EmberAfStatus emberAfColorControlClusterServerCommandParse(EmberAfClusterCommand *cmd);

// Cluster: IAS Zone, client
EmberAfStatus emberAfIasZoneClusterClientCommandParse(EmberAfClusterCommand *cmd);

// Cluster: IAS ACE, client
EmberAfStatus emberAfIasAceClusterClientCommandParse(EmberAfClusterCommand *cmd);

// Cluster: IAS ACE, server
EmberAfStatus emberAfIasAceClusterServerCommandParse(EmberAfClusterCommand *cmd);

// Cluster: Simple Metering, client
EmberAfStatus emberAfSimpleMeteringClusterClientCommandParse(EmberAfClusterCommand *cmd);

#endif // __EMBER_AF_COMMAND_PARSE_HEADER__
