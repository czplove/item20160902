/***************************************************************************//**
 * @file com.h
 * @brief COM Layer.
 * @version 0.01.0
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2014 Silicon Labs, http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/
#ifndef __COM_H__
#define __COM_H__

#include "em_device.h"
#include "em_gpio.h"
#include "uartdrv.h"
#include "hal/micro/cortexm3/usb/em_usb.h" 
#include "com-serial.h"

#ifdef CORTEXM3_EFM32_MICRO
  #include "com_device.h"
  #ifdef COM_RETARGET_SERIAL
    #include "retargetserialconfig.h"
  #endif
#endif

// EM_NUM_SERIAL_PORTS is inherited from the micro specifc micro.h
#if (COM_PORTS == 1)
  #define FOR_EACH_PORT(cast,prefix,suffix)  \
    cast(prefix##_0_##suffix)
  #define DEFINE_COM_HANDLES(name)                   \
    static COM_HandleData_t name##data[COM_PORTS];   \
    COM_Handle_t name[COM_PORTS] = {&name##data[0]};
#elif (COM_PORTS == 2)
  #define FOR_EACH_PORT(cast,prefix,suffix)  \
    cast(prefix##_0_##suffix),                 \
    cast(prefix##_1_##suffix)
  #define DEFINE_COM_HANDLES(name)                   \
    static COM_HandleData_t name##data[COM_PORTS];   \
    COM_Handle_t name[COM_PORTS] = {&name##data[0],&name##data[1]};
#elif (COM_PORTS == 3)
  #define FOR_EACH_PORT(cast,prefix,suffix)  \
    cast(prefix##_0_##suffix),                 \
    cast(prefix##_1_##suffix),                 \
    cast(prefix##_2_##suffix)
  #define DEFINE_COM_HANDLES(name)                   \
    static COM_HandleData_t name##data[COM_PORTS];   \
    COM_Handle_t name[COM_PORTS] = {&name##data[0],&name##data[1],&name##data[2]};
#elif (COM_PORTS == 4)
  #define FOR_EACH_PORT(cast,prefix,suffix)  \
    cast(prefix##_0_##suffix),                 \
    cast(prefix##_1_##suffix),                 \
    cast(prefix##_2_##suffix),                 \
    cast(prefix##_3_##suffix)
  #define DEFINE_COM_HANDLES(name)                   \
    static COM_HandleData_t name##data[COM_PORTS];   \
    COM_Handle_t name[COM_PORTS] = {&name##data[0],&name##data[1],&name##data[2],&name##data[3]};
#else
  #error unsupported number of serial ports
#endif

#if (COM_UART_PORTS == 0)
#define DEFINE_UART_HANDLES(name)
#elif (COM_UART_PORTS == 1)
#define DEFINE_UART_HANDLES(name) \
  static UARTDRV_HandleData_t name##data[COM_UART_PORTS]; \
  UARTDRV_Handle_t name[COM_UART_PORTS] = {&name##data[0]};
#elif (COM_UART_PORTS == 2)
#define DEFINE_UART_HANDLES(name) \
  static UARTDRV_HandleData_t name##data[COM_UART_PORTS]; \
  UARTDRV_Handle_t name[COM_UART_PORTS] = {&name##data[0],&name##data[1]};
#elif (COM_UART_PORTS == 3)
#define DEFINE_UART_HANDLES(name) \
  static UARTDRV_HandleData_t name##data[COM_UART_PORTS]; \
  UARTDRV_Handle_t name[COM_UART_PORTS] = {&name##data[0],&name##data[1],&name##data[2]};
#endif

#ifndef COM_0_RX_QUEUE_SIZE
  #define COM_0_RX_QUEUE_SIZE 64
#endif

#ifndef COM_0_TX_QUEUE_SIZE
  #define COM_0_TX_QUEUE_SIZE 128
#endif

#ifndef COM_1_RX_QUEUE_SIZE
  #define COM_1_RX_QUEUE_SIZE 64
#endif

#ifndef COM_1_TX_QUEUE_SIZE
  #define COM_1_TX_QUEUE_SIZE 128
#endif

#ifndef COM_2_RX_QUEUE_SIZE
  #define COM_2_RX_QUEUE_SIZE 64
#endif

#ifndef COM_2_TX_QUEUE_SIZE
  #define COM_2_TX_QUEUE_SIZE 128
#endif

#ifndef COM_3_RX_QUEUE_SIZE
  #define COM_3_RX_QUEUE_SIZE 64
#endif

#ifndef COM_3_TX_QUEUE_SIZE
  #define COM_3_TX_QUEUE_SIZE 128
#endif

#ifdef COM_USART0_ENABLE

  #if defined( _USART_ROUTELOC0_MASK )
  #ifdef RETARGET_USART0
    #define USART0_TX_LOCATION RETARGET_TX_LOCATION
    #define USART0_RX_LOCATION RETARGET_RX_LOCATION
    #ifdef RETARGET_CTS_PORT
      #define USART0_CTS_PORT RETARGET_CTS_PORT
      #define USART0_CTS_PIN  RETARGET_CTS_PIN
    #endif
    #ifdef RETARGET_RTS_PORT
      #define USART0_RTS_PORT RETARGET_RTS_PORT
      #define USART0_RTS_PIN  RETARGET_RTS_PIN
    #endif
  #endif /*RETARGET_USART0*/

  #ifndef USART0_TX_LOCATION
    #define USART0_TX_LOCATION _USART_ROUTELOC0_TXLOC_LOC1
  #endif
  #ifndef USART0_RX_LOCATION
    #define USART0_RX_LOCATION _USART_ROUTELOC0_RXLOC_LOC31
  #endif 
  #ifndef USART0_CTS_PORT
    #define USART0_CTS_PORT gpioPortA
  #endif
  #ifndef USART0_CTS_PIN
    #define USART0_CTS_PIN 2
  #endif
  #ifndef USART0_RTS_PORT
    #define USART0_RTS_PORT gpioPortA
  #endif
  #ifndef USART0_RTS_PIN
    #define USART0_RTS_PIN 3
  #endif

  #ifndef USART0_CTS_LOCATION 
    #define USART0_CTS_LOCATION _USART_ROUTELOC1_CTSLOC_LOC30
  #endif

  #ifndef USART0_RTS_LOCATION 
    #define USART0_RTS_LOCATION _USART_ROUTELOC1_RTSLOC_LOC30
  #endif

  #ifdef COM_USART0_HW_FC
    #define USART0_FC_MODE uartdrvFlowControlHwUart
  #elif defined (COM_USART0_SW_FC)
    #define USART0_FC_MODE uartdrvFlowControlSw
  #else
    #define USART0_FC_MODE uartdrvFlowControlNone
  #endif

  #define USART0_INIT                                                                                         \
  {                                                                                                               \
    USART0,                                                               /* USART port                   */      \
    115200,                                                               /* Baud rate                    */      \
    USART0_TX_LOCATION,                                                   /* USART Tx pin location number */      \
    USART0_RX_LOCATION,                                                   /* USART Rx pin location number */      \
    (USART_Stopbits_TypeDef)USART_FRAME_STOPBITS_ONE,                     /* Stop bits                    */      \
    (USART_Parity_TypeDef)USART_FRAME_PARITY_NONE,                        /* Parity                       */      \
    (USART_OVS_TypeDef)USART_CTRL_OVS_X16,                                /* Oversampling mode            */      \
    false,                                                                /* Majority vote disable        */      \
    USART0_FC_MODE,                                                       /* Flow control                 */      \
    USART0_CTS_PORT,                                                      /* CTS port number              */      \
    USART0_CTS_PIN,                                                       /* CTS pin number               */      \
    USART0_RTS_PORT,                                                      /* RTS port number              */      \
    USART0_RTS_PIN,                                                       /* RTS pin number               */      \
    NULL,                                                                 /* RX operation queue           */      \
    NULL,                                                                 /* TX operation queue           */      \
    USART0_CTS_LOCATION,                                                  /* CTS pin location             */      \
    USART0_RTS_LOCATION                                                   /* RTS pin location             */      \
  }
  #else //defined( _USART_ROUTELOC0_MASK )

  #if (RETARGET_UART == USART0)
    #define USART0_ROUTE_LOCATION BSP_BCC_LOCATION
  #else /*(BSP_BCC_USART == USART0)*/
    #define USART0_ROUTE_LOCATION _USART_ROUTE_LOCATION_LOC1
  #endif /*(BSP_BCC_USART == USART0)*/

  #define USART0_INIT                                                                                         \
  {                                                                                                               \
    USART0,                                                               /* USART port                   */      \
    115200,                                                               /* Baud rate                    */      \
    USART0_ROUTE_LOCATION,                                                /* USART pins location number   */      \
    (USART_Stopbits_TypeDef)USART_FRAME_STOPBITS_ONE,                     /* Stop bits                    */      \
    (USART_Parity_TypeDef)USART_FRAME_PARITY_NONE,                        /* Parity                       */      \
    (USART_OVS_TypeDef)USART_CTRL_OVS_X16,                                /* Oversampling mode            */      \
    false,                                                                /* Majority vote disable        */      \
    USART0_FC_MODE,                                                       /* Flow control                 */      \
    (GPIO_Port_TypeDef)AF_USART0_CS_PORT(USART0_ROUTE_LOCATION),          /* CTS port number              */      \
    AF_USART0_CS_PIN(USART0_ROUTE_LOCATION),                              /* CTS pin number               */      \
    (GPIO_Port_TypeDef)AF_USART0_CLK_PORT(USART0_ROUTE_LOCATION),         /* RTS port number              */      \
    AF_USART0_CLK_PIN(USART0_ROUTE_LOCATION),                             /* RTS pin number               */      \
    NULL,                                                                 /* RX operation queue           */      \
    NULL                                                                  /* TX operation queue           */      \
  }

  #endif //defined( _USART_ROUTELOC0_MASK )

  #ifndef COM_USART0_RXSTOP
    #define COM_USART0_RXSTOP 17
  #endif
  #ifndef COM_USART0_RXSTART
    #define COM_USART0_RXSTART 21
  #endif

  #define COM_USART0_DEFAULT                                                   \
  {                                                                            \
    (UARTDRV_Init_t) USART0_INIT,                  /* USART initdata        */ \
    COM_USART0_RXSTOP,                             /* RX stop threshold     */ \
    COM_USART0_RXSTART                             /* RX start threshold    */ \
  }
#endif //COM_USART0_ENABLE
#ifdef COM_USART1_ENABLE
  #if defined( _USART_ROUTELOC0_MASK )

  #ifdef RETARGET_USART1
    #define USART1_TX_LOCATION RETARGET_TX_LOCATION
    #define USART1_RX_LOCATION RETARGET_RX_LOCATION
    #ifdef RETARGET_CTS_PORT
      #define USART1_CTS_PORT RETARGET_CTS_PORT
      #define USART1_CTS_PIN  RETARGET_CTS_PIN
    #endif
    #ifdef RETARGET_RTS_PORT
      #define USART1_RTS_PORT RETARGET_RTS_PORT
      #define USART1_RTS_PIN  RETARGET_RTS_PIN
    #endif
  #endif /*RETARGET_USART1*/

  #ifndef USART1_TX_LOCATION
    #define USART1_TX_LOCATION _USART_ROUTELOC0_TXLOC_LOC3
  #endif
  #ifndef USART1_RX_LOCATION
    #define USART1_RX_LOCATION _USART_ROUTELOC0_RXLOC_LOC1
  #endif 
  #ifndef USART1_CTS_PORT
    #define USART1_CTS_PORT gpioPortD
  #endif
  #ifndef USART1_CTS_PIN
    #define USART1_CTS_PIN 12
  #endif
  #ifndef USART1_RTS_PORT
    #define USART1_RTS_PORT gpioPortD
  #endif
  #ifndef USART1_RTS_PIN
    #define USART1_RTS_PIN 11
  #endif

  #ifdef COM_USART1_HW_FC
    #define USART1_FC_MODE uartdrvFlowControlHw
  #elif defined (COM_USART1_SW_FC)
    #define USART1_FC_MODE uartdrvFlowControlSw
  #else
    #define USART1_FC_MODE uartdrvFlowControlNone
  #endif

  #define USART1_INIT                                                                                         \
  {                                                                                                               \
    USART1,                                                               /* USART port                   */      \
    115200,                                                               /* Baud rate                    */      \
    USART1_TX_LOCATION,                                                   /* USART Tx pin location number */      \
    USART1_RX_LOCATION,                                                   /* USART Rx pin location number */      \
    (USART_Stopbits_TypeDef)USART_FRAME_STOPBITS_ONE,                     /* Stop bits                    */      \
    (USART_Parity_TypeDef)USART_FRAME_PARITY_NONE,                        /* Parity                       */      \
    (USART_OVS_TypeDef)USART_CTRL_OVS_X16,                                /* Oversampling mode            */      \
    false,                                                                /* Majority vote disable        */      \
    USART1_FC_MODE,                                                       /* Flow control                 */      \
    USART1_CTS_PORT,                                                      /* CTS port number              */      \
    USART1_CTS_PIN,                                                       /* CTS pin number               */      \
    USART1_RTS_PORT,                                                      /* RTS port number              */      \
    USART1_RTS_PIN,                                                       /* RTS pin number               */      \
    NULL,                                                                 /* RX operation queue           */      \
    NULL                                                                  /* TX operation queue           */      \
  }
  #else //defined( _USART_ROUTELOC0_MASK )

  #define USART1_INIT                                                                                         \
  {                                                                                                               \
    USART1,                                                               /* USART port                   */      \
    115200,                                                               /* Baud rate                    */      \
    _USART_ROUTE_LOCATION_LOC1,                                           /* USART pins location number   */      \
    (USART_Stopbits_TypeDef)USART_FRAME_STOPBITS_ONE,                     /* Stop bits                    */      \
    (USART_Parity_TypeDef)USART_FRAME_PARITY_NONE,                        /* Parity                       */      \
    (USART_OVS_TypeDef)USART_CTRL_OVS_X16,                                /* Oversampling mode            */      \
    false,                                                                /* Majority vote disable        */      \
    USART1_FC_MODE,                                                       /* Flow control                 */      \
    (GPIO_Port_TypeDef)AF_USART1_CS_PORT(_USART_ROUTE_LOCATION_LOC1),     /* CTS port number              */      \
    AF_USART1_CS_PIN(_USART_ROUTE_LOCATION_LOC1),                         /* CTS pin number               */      \
    (GPIO_Port_TypeDef)AF_USART1_CLK_PORT(_USART_ROUTE_LOCATION_LOC1),    /* RTS port number              */      \
    AF_USART1_CLK_PIN(_USART_ROUTE_LOCATION_LOC1),                        /* RTS pin number               */      \
    NULL,                                                                 /* RX operation queue           */      \
    NULL                                                                  /* TX operation queue           */      \
  }
  #endif //defined( _USART_ROUTELOC0_MASK )

  #ifndef COM_USART1_RXSTOP
    #define COM_USART1_RXSTOP 17
  #endif
  #ifndef COM_USART1_RXSTART
    #define COM_USART1_RXSTART 21
  #endif

  #define COM_USART1_DEFAULT                                                   \
  {                                                                            \
    (UARTDRV_Init_t) USART1_INIT,                  /* USART initdata        */ \
    COM_USART1_RXSTOP,                             /* RX stop threshold     */ \
    COM_USART1_RXSTART                             /* RX start threshold    */ \
  }
#endif //COM_USART1_ENABLE

#ifdef COM_USART2_ENABLE

  #define USART2_INIT                                                                                         \
  {                                                                                                               \
    USART2,                                                               /* USART port                   */      \
    115200,                                                               /* Baud rate                    */      \
    _USART_ROUTE_LOCATION_LOC1,                                           /* USART pins location number   */      \
    (USART_Stopbits_TypeDef)USART_FRAME_STOPBITS_ONE,                     /* Stop bits                    */      \
    (USART_Parity_TypeDef)USART_FRAME_PARITY_NONE,                        /* Parity                       */      \
    (USART_OVS_TypeDef)USART_CTRL_OVS_X16,                                /* Oversampling mode            */      \
    false,                                                                /* Majority vote disable        */      \
    uartdrvFlowControlHw,                                                 /* Flow control                 */      \
    (GPIO_Port_TypeDef)AF_USART2_CS_PORT(_USART_ROUTE_LOCATION_LOC1),     /* CTS port number              */      \
    (GPIO_Port_TypeDef)AF_USART2_CS_PIN(_USART_ROUTE_LOCATION_LOC1),      /* CTS pin number               */      \
    (GPIO_Port_TypeDef)AF_USART2_CLK_PORT(_USART_ROUTE_LOCATION_LOC1),    /* RTS port number              */      \
    (GPIO_Port_TypeDef)AF_USART2_CLK_PIN(_USART_ROUTE_LOCATION_LOC1),     /* RTS pin number               */      \
    NULL,                                                                 /* RX operation queue           */      \
    NULL                                                                  /* TX operation queue           */      \
  }
  #ifndef COM_USART2_RXSTOP
    #define COM_USART2_RXSTOP 17
  #endif
  #ifndef COM_USART2_RXSTART
    #define COM_USART2_RXSTART 21
  #endif

  #define COM_USART2_DEFAULT                                                   \
  {                                                                            \
    (UARTDRV_Init_t) USART2_INIT,                  /* USART initdata        */ \
    COM_USART2_RXSTOP,                             /* RX stop threshold     */ \
    COM_USART2_RXSTART                             /* RX start threshold    */ \
  }
#endif //COM_USART2_ENABLE

// Enable individual com ports
typedef enum COM_Port
{
  COM_VCP = 0,
  COM_USART0 = 1, 
  COM_USART1 = 2,
  COM_USART2 = 3,
  COM_USB = 4
} COM_Port_t;
#ifdef __CC_ARM
#pragma anon_unions
#endif
typedef struct
{
  UARTDRV_Init_t uartdrvinit;
  uint16_t rxStop;
  uint16_t rxStart;
} COM_Init_t;

typedef struct {
  /** Index of next byte to send.*/
  uint16_t head;
  /** Index of where to enqueue next message.*/
  uint16_t tail;
  /** Number of bytes queued.*/
  volatile uint16_t used;
  /*! Number of bytes pumped */
  uint16_t pumped;
  /** FIFO of queue data.*/
  uint8_t fifo[];
} COM_FifoQueue_t;

typedef struct COM_HandleData
{
  COM_FifoQueue_t *rxQueue;
  COM_FifoQueue_t *txQueue;
  uint16_t rxsize;
  uint16_t txsize;
  bool txPaused;
  bool txCatchUp;
  uint16_t rxStop;
  uint16_t rxStart;
  UARTDRV_Handle_t uarthandle;
} COM_HandleData_t;

/// Handle pointer
typedef COM_HandleData_t * COM_Handle_t;

//------------------------------------------------------
// Memory allocations for Queue data structures

//Macros to define fifo and buffer queues, can't use a typedef becuase the size
// of the fifo array in the queues can change
#define DEFINE_FIFO_QUEUE(qSize, qName)             \
  static struct {                                   \
    /*! Indexes of next byte to send*/              \
    uint16_t head;                                  \
    /*! Index of where to enqueue next message*/    \
    uint16_t tail;                                  \
    /*! Number of bytes queued*/                    \
    volatile uint16_t used;                         \
    /*! Number of bytes pumped */                   \
    uint16_t pumped;                                \
    /*! FIFO of queue data*/                        \
    uint8_t fifo[qSize];                            \
  } qName;

#undef  FIFO_ENQUEUE // Avoid possible warning, replace other definition
#define FIFO_ENQUEUE(queue,data,size)               \
  do {                                              \
    (queue)->fifo[(queue)->head] = (data);          \
    (queue)->head = (((queue)->head + 1) % (size)); \
    (queue)->used++;                                \
  } while(0)
#undef  FIFO_DEQUEUE // Avoid possible warning, replace other definition
#define FIFO_DEQUEUE(queue,size)                    \
  (queue)->fifo[(queue)->tail];                     \
  (queue)->tail = (((queue)->tail + 1) % (size));   \
  (queue)->used--
#define FIFO_DEQUEUE_MULTIPLE(queue,size,num) \
  queue->tail = ((queue->tail + num) % (size));  \
  queue->used -= num

#ifndef halEnableVCOM
  #define halEnableVCOM()
#endif


#define COM_INITUART(initdata) ((COM_Init_t) initdata)

void COM_InternalPowerDown();
void COM_InternalPowerUp();
bool COM_InternalTxIsIdle(COM_Port_t port);
bool COM_InternalRxIsPaused(COM_Port_t port);
bool COM_InternalTxIsPaused(COM_Port_t port);
void pumpRx(COM_Port_t port);
void updateBuffer(COM_FifoQueue_t *q, uint16_t xferred, uint16_t size);
int reloadUsbBuffer(USB_Status_TypeDef status, uint32_t xferred, uint32_t remaining );

Ecode_t COM_Init(COM_Port_t port, COM_Init_t *init);
Ecode_t COM_DeInit(COM_Port_t port);

uint16_t COM_ReadAvailable(COM_Port_t port);
Ecode_t COM_ReadByte(COM_Port_t port, uint8_t *dataByte);
Ecode_t COM_ReadData(COM_Port_t port,
                                uint8_t *data,
                                uint16_t length,
                                uint16_t *bytesRead);
Ecode_t COM_ReadDataTimeout(COM_Port_t port,
                                       uint8_t *data,
                                       uint16_t length,
                                       uint16_t *bytesRead,
                                       uint16_t firstByteTimeout,
                                       uint16_t subsequentByteTimeout);
Ecode_t COM_ReadPartialLine(COM_Port_t port, char *data, uint8_t max, uint8_t * index);
Ecode_t COM_ReadLine(COM_Port_t port, char *data, uint8_t max);

Ecode_t COM_ForceWriteData(COM_Port_t port, uint8_t *data, uint8_t length);
uint16_t COM_WriteAvailable(COM_Port_t port);
uint16_t COM_WriteUsed(COM_Port_t port);
Ecode_t COM_WriteByte(COM_Port_t port, uint8_t dataByte);
Ecode_t COM_WriteHex(COM_Port_t port, uint8_t dataByte);
Ecode_t COM_WriteString(COM_Port_t port, PGM_P string);
Ecode_t COM_PrintCarriageReturn(COM_Port_t port);
Ecode_t COM_Printf(COM_Port_t port, PGM_P formatString, ...);
Ecode_t COM_PrintfVarArg(COM_Port_t port, PGM_P formatString, va_list ap);
Ecode_t COM_PrintfLine(COM_Port_t port, PGM_P formatString, ...);
Ecode_t COM_WriteData(COM_Port_t port, uint8_t *data, uint8_t length);
Ecode_t COM_GuaranteedPrintf(COM_Port_t port, PGM_P formatString, ...);
Ecode_t COM_WaitSend(COM_Port_t port);
void COM_FlushRx(COM_Port_t port);
bool COM_Unused(uint8_t port);

#endif //__COM_H__
