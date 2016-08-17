// *******************************************************************
// * rules-engine.h
// *
// *
// * Copyright 2014 by Silicon Labs. All rights reserved.         *80*
// *******************************************************************

#ifndef __RULES_ENGINE_H
#define __RULES_ENGINE_H

// bindings
// incoming command gets sent to a device as-is.  

typedef struct {
  uint16_t inAddressTableEntry;
  uint8_t  inEndpoint;
  uint16_t outAddressTableEntry;
  uint8_t  outEndpoint;
} BindEntry;

typedef struct
{
  uint16_t currentX, currentY;
  uint16_t currentMiredTemp;  // temperature attribute is 10^6 / Kelvin
  uint8_t  currentHue;
  uint16_t deviceEntry;       // track the device entry to handle multiple binds
} BulbEntry;

// rules
// incoming command triggers an arbitrary outgoing command
enum {
  IN_ACTION_OFF       = 0,
  IN_ACTION_ON        = 1,
  IN_ACTION_ALARM_ON  = 2,
  IN_ACTION_ALARM_OFF = 3,
  IN_ACTION_TIME      = 4,
  IN_ACTION_NONE      = 0xff
};
enum {
  OUT_ACTION_OFF         = 0,
  OUT_ACTION_ON          = 1,
  OUT_ACTION_LEVEL       = 2,
  OUT_ACTION_LOCK        = 3,
  OUT_ACTION_UNLOCK      = 4,
  OUT_ACTION_COLOR_XY    = 5,
  OUT_ACTION_PARTY       = 6, // send random color
};

typedef struct {
  uint16_t colorX;
  uint16_t colorY;
} ColorXy;

typedef union {
  uint8_t level;
  uint32_t timeOfDay;
  ColorXy colorXy;
  uint16_t data;
} ActionData;

typedef struct {
  uint16_t inAddressTableEntry;
  uint8_t  inEndpoint;
  uint16_t outAddressTableEntry;
  uint8_t  outEndpoint;
  uint8_t  inAction;
  uint8_t  outAction;

  ActionData inActionData;
  ActionData outActionData;

} ActionEntry;

void processActionsCmd(EmberAfClusterCommand* cmd, uint8_t inEndpoint);
void initBindTable( void );
void rulesEnginePrintMqtt( void );
void emAfPluginRulesEngineInitActions( void );

#define ACTION_TABLE_SIZE 1000
#define BIND_TABLE_SIZE 200
#define UNDEFINED_ENDPOINT 0xFF
#define NULL_ENTRY 0xffff
#define ATTRIBUTE_BUFFER_DATA_OFFSET  3
#define ATTRIBUTE_BUFFER_ID_OFFSET_L  1
#define ATTRIBUTE_BUFFER_ID_OFFSET_U  0

extern BindEntry bindTable[BIND_TABLE_SIZE];
extern BulbEntry bulbTable[BIND_TABLE_SIZE];

void emberAfPluginRulesEngineChangedCallback(void);

void emberAfRulesEngineReportAttributeRespones(EmberAfClusterId clusterId,
                                               uint8_t * buffer,
                                               uint16_t bufLen);
void emberAfPluginRulesEngineKillRule(void);

extern BindEntry bindTable[];

#endif //__RULES_ENGINE_H
