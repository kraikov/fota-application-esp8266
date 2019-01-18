#ifndef UART_APP_H
#define UART_APP_H

//----------------------------------------------------------------------------------------------------------------------
// Included files to resolve specific definitions in this file
//----------------------------------------------------------------------------------------------------------------------
#include "uart_register.h"
#include "eagle_soc.h"
#include "c_types.h"

//----------------------------------------------------------------------------------------------------------------------
// Constant data
//----------------------------------------------------------------------------------------------------------------------
#define RX_BUFF_SIZE    0x100
#define TX_BUFF_SIZE    100

//----------------------------------------------------------------------------------------------------------------------
// Exported type
//----------------------------------------------------------------------------------------------------------------------
typedef enum
{
    FIVE_BITS = 0x0,
    SIX_BITS = 0x1,
    SEVEN_BITS = 0x2,
    EIGHT_BITS = 0x3
} UartBitsNum4Char;

typedef enum
{
    ONE_STOP_BIT = 0,
    ONE_HALF_STOP_BIT = BIT2,
    TWO_STOP_BIT = BIT2
} UartStopBitsNum;

typedef enum
{
    NONE_BITS = 0,
    ODD_BITS = 0,
    EVEN_BITS = BIT4
} UartParityMode;

typedef enum
{
    STICK_PARITY_DIS = 0,
    STICK_PARITY_EN = BIT3 | BIT5
} UartExistParity;

typedef enum
{
    BIT_RATE_9600 = 9600,
    BIT_RATE_19200 = 19200,
    BIT_RATE_38400 = 38400,
    BIT_RATE_57600 = 57600,
    BIT_RATE_74880 = 74880,
    BIT_RATE_115200 = 115200,
    BIT_RATE_230400 = 230400,
    BIT_RATE_460800 = 460800,
    BIT_RATE_921600 = 921600
} UartBautRate;

typedef enum
{
    NONE_CTRL,
    HARDWARE_CTRL,
    XON_XOFF_CTRL
} UartFlowControl;

typedef enum
{
    EMPTY,
    UNDER_WRITE,
    WRITE_OVER
} ReceivedMessageBufferState;

typedef struct
{
    uint32 ReceivedBufferSize;
    uint8* ReceivedMessageBuffer;
    uint8* WritePosition;
    uint8* ReadPosition;
    uint8 TriggerLevel;
    ReceivedMessageBufferState BufferState;
} ReceivedMessageBuffer;

typedef struct
{
    uint32 TransmittedBufferSize;
    uint8* TransmittedBuffer;
} TransmittedMessageBuffer;

typedef enum
{
    BAUD_RATE_DET,
    WAIT_SYNC_FRM,
    SRCH_MSG_HEAD,
    RCV_MSG_BODY,
    RCV_ESC_CHAR,
} ReceivedMessageState;

typedef struct
{
    UartBautRate BautRate;
    UartBitsNum4Char DataBits;
    UartExistParity ExistParity;
    UartParityMode Parity;
    UartStopBitsNum StopBits;
    UartFlowControl FlowControl;
    ReceivedMessageBuffer ReceivedBuffer;
    TransmittedMessageBuffer TransmittedBuffer;
    ReceivedMessageState ReceivedState;
    int Received;
    int UartModuleNumber;
} UartDevice;

//======================================================================================================================
// EXPORTED FUNCTIONS
//======================================================================================================================
void UART_Init(UartBautRate uart0_BautRate, UartBautRate uart1_BautRate);

void UART0_Send(char *buffer);

void UART0_TransmitBuffer(uint8* buffer, uint16 length);

extern void ParseCommand(char* string);

#endif

