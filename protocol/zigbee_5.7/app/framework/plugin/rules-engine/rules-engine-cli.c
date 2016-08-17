// *******************************************************************
// * rules-engine-cli.c
// *
// *
// * Copyright 2012 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/util/serial/command-interpreter2.h"
#include "rules-engine.h"
#include "../device-table/device-table.h"
#include "stack/include/ember-types.h"

#ifdef UNIX
  #define emberAfCorePrint emberSerialPrintf
  #define emberAfCorePrintln emberSerialPrintfLine
#endif

extern BindEntry bindTable[];

#define SIZE_TIME_OF_DAY 4
#define SIZE_LEVEL 1
#define SIZE_COLOR_XY 4

#if !defined(EMBER_AF_GENERATE_CLI)

void emberAfPluginRulesEngineBindCommand(void); 
void emberAfPluginRulesEngineActionCommand(void);

EmberCommandEntry emberAfPluginRulesEngineCommands[] = {
  emberCommandEntryAction("bind", emberAfPluginRulesEngineBindCommand, "vv", ""),
  emberCommandEntryAction("action", emberAfPluginRulesEngineActionCommand, "vvuuu", ""),
  emberCommandEntryTerminator(),
};
#endif // EMBER_AF_GENERATE_CLI
void addBindEntry(uint16_t inAddressTableEntry, uint16_t outAddressTableEntry);
uint16_t findBlankAction(void);
void rulesEngineSaveCommand(void);
void rulesEngineBindSaveCommand(void);

void emberAfPluginRulesEngineBindCommand(void)
{
  uint16_t inAddressTableEntry = (uint16_t) emberUnsignedCommandArgument(0);
  uint16_t outAddressTableEntry = (uint16_t) emberUnsignedCommandArgument(1);

  addBindEntry(inAddressTableEntry, outAddressTableEntry);
}

void emberAfPluginRulesEngineEndpointBindCommand(void)
{
  uint16_t inAddressTableEntry = (uint16_t) emberUnsignedCommandArgument(0);
  uint8_t  inEndpoint = (uint8_t) emberUnsignedCommandArgument(1);
  uint16_t outAddressTableEntry = (uint16_t) emberUnsignedCommandArgument(2);
  uint8_t  outEndpoint = (uint8_t) emberUnsignedCommandArgument(3);

  addBindEntryEndpoint(inAddressTableEntry, 
                       inEndpoint, 
                       outAddressTableEntry, 
                       outEndpoint);
}


extern ActionEntry actionTable[];

void emberAfPluginRulesEngineActionCommand(void) 
{
  uint16_t inAddressTableEntry  = (uint16_t) emberUnsignedCommandArgument(0);
  uint16_t outAddressTableEntry = (uint16_t) emberUnsignedCommandArgument(1);
  uint8_t inAction  = (uint8_t) emberUnsignedCommandArgument(2);
  uint8_t outAction = (uint8_t) emberUnsignedCommandArgument(3);

  uint16_t i = findBlankAction();

  actionTable[i].inAddressTableEntry = inAddressTableEntry;
  actionTable[i].outAddressTableEntry = outAddressTableEntry;
  actionTable[i].inAction = inAction;
  actionTable[i].outAction = outAction;

  // handle data
  switch (inAction) {
  case IN_ACTION_TIME:
    emberCopyStringArgument(4, // arg number
                            ((uint8_t *) &(actionTable[i].inActionData.timeOfDay)),
                            SIZE_TIME_OF_DAY, // size
                            FALSE); // false means no left pad
    break;
  }

  switch (outAction) {
  case OUT_ACTION_LEVEL:
    emberCopyStringArgument(5,
                            &(actionTable[i].outActionData.level),
                            SIZE_LEVEL,
                            FALSE);
    break;
  case OUT_ACTION_COLOR_XY:
    emberCopyStringArgument(5, 
                            ((uint8_t *) &(actionTable[i].outActionData)),
                            SIZE_COLOR_XY,
                            FALSE);

    emberAfCorePrintln(APP_SERIAL, "x: %2x, y: %2x",
                      actionTable[i].outActionData.colorXy.colorX,
                      actionTable[i].outActionData.colorXy.colorY);
    break;
  }

  // Save on new rule
  rulesEngineBindSaveCommand();
  rulesEngineSaveCommand();
  emberAfPluginRulesEngineChangedCallback();
}

// Add an entry into the rules engine actions list that includes an 
// incoming and outgoing endpoint.
void emberAfPluginRulesEngineEndpointActionCommand(void) 
{
  uint16_t inAddressTableEntry  = (uint16_t) emberUnsignedCommandArgument(0);
  uint8_t inEndpoint = (uint8_t) emberUnsignedCommandArgument(1);
  uint16_t outAddressTableEntry = (uint16_t) emberUnsignedCommandArgument(2);
  uint8_t outEndpoint = (uint8_t) emberUnsignedCommandArgument(3);
  uint8_t inAction  = (uint8_t) emberUnsignedCommandArgument(4);
  uint8_t outAction = (uint8_t) emberUnsignedCommandArgument(5);

  uint16_t i = findBlankAction();

  actionTable[i].inAddressTableEntry = inAddressTableEntry;
  actionTable[i].inEndpoint = inEndpoint;
  actionTable[i].outAddressTableEntry = outAddressTableEntry;
  actionTable[i].outEndpoint = outEndpoint;
  actionTable[i].inAction = inAction;
  actionTable[i].outAction = outAction;

  // Some actions have more complicated data.  For these actions, we need to 
  // pass an arbitrary length byte array.  This is handled locally as a string
  // copy.  
  switch (inAction) {
  case IN_ACTION_TIME:
    emberCopyStringArgument(6, // arg number
                            ((uint8_t *) &(actionTable[i].inActionData.timeOfDay)),
                            SIZE_TIME_OF_DAY, // size of the time of day argument.
                            FALSE); // false means no left pad
    break;
  }

  switch (outAction) {
  case OUT_ACTION_LEVEL:
    emberCopyStringArgument(7,
                            &(actionTable[i].outActionData.level),
                            SIZE_LEVEL,
                            FALSE);
    break;
  case OUT_ACTION_COLOR_XY:
    emberCopyStringArgument(7, 
                            ((uint8_t *) &(actionTable[i].outActionData)),
                            SIZE_COLOR_XY,
                            FALSE);

    emberAfCorePrintln(APP_SERIAL, "x: %2x, y: %2x",
                      actionTable[i].outActionData.colorXy.colorX,
                      actionTable[i].outActionData.colorXy.colorY);
    break;
  }

  // Save on new endpoint rule
  rulesEngineBindSaveCommand();
  rulesEngineSaveCommand();
  emberAfPluginRulesEngineChangedCallback();
}

// ****************************************************************************
// 
// Save/load of the device table. 
#ifndef EMBEDDED_GATEWAY
  #include <stdio.h>
#endif

void rulesEngineSaveCommand(void)
{
#ifndef EMBEDDED_GATEWAY
  FILE *fp;
  uint16_t i; 

  fp = fopen("rules-actions.txt", "w");
  
  for (i = 0; i < ACTION_TABLE_SIZE; i++) {
    if (actionTable[i].inAction != 0xff) {
    fprintf(fp, "%d %d %d %d ",
              actionTable[i].inAddressTableEntry,
              actionTable[i].inEndpoint,
              actionTable[i].outAddressTableEntry,
              actionTable[i].outEndpoint,
              actionTable[i].inAction,
              actionTable[i].outAction);

      switch (actionTable[i].inAction) {
      case IN_ACTION_TIME:
        fprintf(fp, "%ld ", (long int) actionTable[i].inActionData.timeOfDay);
        break;
      }

      switch (actionTable[i].outAction) {
      case OUT_ACTION_LEVEL:
        fprintf(fp, "%d", actionTable[i].outActionData.level);
        break;
      case OUT_ACTION_COLOR_XY:
        fprintf(fp, "%d %d", 
                actionTable[i].outActionData.colorXy.colorX,
                actionTable[i].outActionData.colorXy.colorY);
        emberAfCorePrint(APP_SERIAL, "%d %d", 
                actionTable[i].outActionData.colorXy.colorX,
                actionTable[i].outActionData.colorXy.colorY);
        break;
      }
      fprintf(fp, "\r\n");

    }
  }

  // write something to mark the end
  fprintf(fp, "ffff\r\n");
  fclose(fp);
#endif
}

extern void initActions(void);

void rulesEngineLoadCommand(void)
{
#ifndef EMBEDDED_GATEWAY
  FILE *fp;
  uint16_t i, j;
  unsigned int data;
  unsigned long int dataLong;

  emAfPluginRulesEngineInitActions();

  fp = fopen("rules-actions.txt", "r");

  if (!fp) {
    return;
  }

  j = 1;
 
  for (i = 0; i < ACTION_TABLE_SIZE && j == 1; i++) {
    j = fscanf(fp, "%d", &data);
    actionTable[i].inAddressTableEntry = (uint16_t) data;
    fscanf(fp, "%d", &data);
    actionTable[i].inEndpoint = (uint8_t) data;
    fscanf(fp, "%d", &data);
    actionTable[i].outAddressTableEntry = (uint16_t) data;
    fscanf(fp, "%d", &data);
    actionTable[i].outEndpoint = (uint8_t) data;

    if (j == 1 && actionTable[i].inAddressTableEntry != 0xffff) {
      fscanf(fp, "%d ", &data);
      actionTable[i].inAction = (uint8_t) data;
      fscanf(fp, "%d ", &data);
      actionTable[i].outAction = (uint8_t) data;
       
      switch (actionTable[i].inAction) {
      case IN_ACTION_TIME:
        fscanf(fp, "%ld ", &(actionTable[i].inActionData.timeOfDay));
        break;
      }

      switch (actionTable[i].outAction) {
      case OUT_ACTION_LEVEL:
        fscanf(fp, "%d", &data);
               actionTable[i].inActionData.level = (uint8_t) data;
        break;
      case OUT_ACTION_COLOR_XY:
        fscanf(fp, "%d %d", 
               &(actionTable[i].outActionData.colorXy.colorX), 
               &(actionTable[i].outActionData.colorXy.colorY));
        break;
      }
    } else {
      actionTable[i].inAddressTableEntry = 0xffff;
    }
  }
  
  fclose(fp);
#endif
}

extern BindEntry bindTable[];

void rulesEngineBindSaveCommand(void)
{
#ifndef EMBEDDED_GATEWAY
  FILE *fp;
  uint16_t i, j; 

  fp = fopen("rules-binds.txt", "w");
  
  for (i = 0; i < BIND_TABLE_SIZE; i++) {
    if (bindTable[i].inAddressTableEntry != 0xffff) {
    fprintf(fp, "%d %d %d %d\r\n",
              bindTable[i].inAddressTableEntry,
              bindTable[i].inEndpoint,
              bindTable[i].outAddressTableEntry,
              bindTable[i].outEndpoint);
    }
  }

  // write something to mark the end
  fprintf(fp, "ffff\r\n");
  fclose(fp);
#endif
}

void rulesEngineBindLoadCommand(void)
{
#ifndef EMBEDDED_GATEWAY
  FILE *fp;
  uint16_t i, j;
  unsigned int data;
  unsigned long int dataLong;

  initBindTable();

  fp = fopen("rules-binds.txt", "r");

  if (!fp) {
    return;
  }

  j = 1;
  
  for (i = 0; i < BIND_TABLE_SIZE && j == 1; i++) {
    j = fscanf(fp, "%d", &data);
    bindTable[i].inAddressTableEntry = (uint16_t) data;
    fscanf(fp, "%d", &data);
    bindTable[i].inEndpoint = (uint8_t) data;
    fscanf(fp, "%d", &data);
    bindTable[i].outAddressTableEntry = (uint16_t) data;
    fscanf(fp, "%d", &data);
    bindTable[i].outEndpoint = (uint8_t) data;

    if (j == 1 && bindTable[i].inAddressTableEntry != 0xffff) {
     // nothing more to do for bind table
     } else {
      bindTable[i].inAddressTableEntry = 0xffff;
      bindTable[i].outAddressTableEntry = 0xffff;
    }
  }
  
  fclose(fp);

#endif
}


void printInAction( uint8_t inAction, ActionData actionData )
{
  emberAfCorePrint(APP_SERIAL, "IN:");
  switch (inAction) {
  case IN_ACTION_OFF:
    emberAfCorePrint(APP_SERIAL, "OFF");
    break;
  case IN_ACTION_ON:
    emberAfCorePrint(APP_SERIAL, "ON");
    break;
  case IN_ACTION_ALARM_OFF:
    emberAfCorePrint(APP_SERIAL, "ALARM_OFF");
    break;
  case IN_ACTION_ALARM_ON:
    emberAfCorePrint(APP_SERIAL, "ALARM_ON");
    break;
  case IN_ACTION_TIME:
    emberAfCorePrint(APP_SERIAL, "TIME");
    emberAfCorePrint(APP_SERIAL, " %ld", actionData.timeOfDay);
    break;
  }
}

void printOutAction( uint8_t outAction, ActionData actionData )
{
  emberAfCorePrint(APP_SERIAL, "OUT:");
  switch (outAction) {
  case OUT_ACTION_OFF:
    emberAfCorePrint(APP_SERIAL, "OFF");
    break;
  case OUT_ACTION_ON:
    emberAfCorePrint(APP_SERIAL, "ON");
    break;
  case OUT_ACTION_LEVEL:
    emberAfCorePrint(APP_SERIAL, "LEVEL");
    emberAfCorePrint(APP_SERIAL, " %x", actionData.level);
    break;
  case OUT_ACTION_LOCK:
    emberAfCorePrint(APP_SERIAL, "LOCK");
    break;
  case OUT_ACTION_UNLOCK:
    emberAfCorePrint(APP_SERIAL, "UNLCOK");
    break;
  case OUT_ACTION_COLOR_XY:
    emberAfCorePrint(APP_SERIAL, "COLOR_XY");
    emberAfCorePrint(APP_SERIAL, " %d %d",
      actionData.colorXy.colorX, actionData.colorXy.colorY);
    break;
  case OUT_ACTION_PARTY:
    emberAfCorePrint(APP_SERIAL, "PARTY_MODE");
    break;
  }
}

void rulesEnginePrintCommand(void)
{
  uint16_t i;

  for (i = 0; i < ACTION_TABLE_SIZE ; i++) {
    if (actionTable[i].inAction != 0xff) {
      emberAfCorePrint(APP_SERIAL, "%2x:%x %2x%x ",
        actionTable[i].inAddressTableEntry,
        actionTable[i].inEndpoint,
        actionTable[i].outAddressTableEntry,
        actionTable[i].outEndpoint);
      printInAction( actionTable[i].inAction, actionTable[i].inActionData );
      emberAfCorePrint(APP_SERIAL, " ");
      printOutAction( actionTable[i].outAction, actionTable[i].outActionData );
      emberAfCorePrintln(APP_SERIAL, "");
    }
  }
}

void rulesEngineBprintCommand(void)
{
  uint16_t i;
  for (i = 0; i < BIND_TABLE_SIZE; i++) {
    if (bindTable[i].inAddressTableEntry != 0xffff) {
      emberAfCorePrintln(APP_SERIAL, "%d 0x%2x:%x 0x%2x:%x",
        i,
        bindTable[i].inAddressTableEntry,
        bindTable[i].inEndpoint,
        bindTable[i].outAddressTableEntry,
        bindTable[i].outEndpoint);
    }
  }
}

void rulesEngineBclearCommand(void)
{
  uint16_t i;
  for (i = 0; i < BIND_TABLE_SIZE; i++) {
    bindTable[i].inAddressTableEntry = NULL_ENTRY;
    bindTable[i].outAddressTableEntry = NULL_ENTRY;
  }

  rulesEngineBindSaveCommand();
  rulesEngineSaveCommand();
  emberAfPluginRulesEngineChangedCallback();
}

void rulesEngineBdeleteCommand(void)
{
  uint16_t i;
  uint16_t inAddressTableEntry = (uint16_t) emberUnsignedCommandArgument(0);
  uint16_t outAddressTableEntry = (uint16_t) emberUnsignedCommandArgument(1);

  for (i = 0; i < BIND_TABLE_SIZE; i++) {
    if (bindTable[i].inAddressTableEntry == inAddressTableEntry && 
      bindTable[i].outAddressTableEntry == outAddressTableEntry) {
      bindTable[i].inAddressTableEntry = NULL_ENTRY;
      bindTable[i].outAddressTableEntry = NULL_ENTRY;
    }
  }

  rulesEngineBindSaveCommand();
  rulesEngineSaveCommand();
  emberAfPluginRulesEngineChangedCallback();
}

