//----------------------------------------------------------------------------------------------------------------------
// Included files to resolve specific definitions in this file
//----------------------------------------------------------------------------------------------------------------------
#include <c_types.h>
#include "ets_sys.h"
#include "osapi.h"
#include "UART_APP.h"
#include <mem.h>

//----------------------------------------------------------------------------------------------------------------------
// Constant data
//----------------------------------------------------------------------------------------------------------------------
#define UART_0   0

#define UART_1   1

extern UartDevice UartDev;

//----------------------------------------------------------------------------------------------------------------------
// Local function prototypes
//----------------------------------------------------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR UART_Configuration(uint8 uartModule);

static STATUS ICACHE_FLASH_ATTR UART1_TransmitChar(uint8 txChar);

static void ICACHE_FLASH_ATTR UART_1_WriteCharacter(char c);

static void UART0_InterruptHandler(void* arg);

static int ICACHE_FLASH_ATTR UART0_ReceiveCharacter();

//======================================================================================================================
// EXPORTED FUNCTIONS
//======================================================================================================================

//======================================================================================================================
// DESCRIPTION:         use UART0 to transfer buffer
//
// PARAMETERS:          uint8 *buf - points to send buffer
//                      uint16 len - buffer length
//
// RETURN VALUE:        void
//
//======================================================================================================================
void ICACHE_FLASH_ATTR UART0_TransmitBuffer(uint8* buffer, uint16 length)
{
    uint16 index;

    for (index = 0; index < length; index++)
    {
        uart_tx_one_char(buffer[index]);
    }
}

//======================================================================================================================
// DESCRIPTION:         use UART0 to transfer buffer
//
// PARAMETERS:          uint8 *buf - points to send buffer
//                      uint16 len - buffer length
//
// RETURN VALUE:        void
//
//======================================================================================================================
void ICACHE_FLASH_ATTR UART0_Send(char* string)
{
    uint8 index = 0;
    while (string[index])
    {
        uart_tx_one_char(string[index]);
        index++;
    }
}

//======================================================================================================================
// DESCRIPTION:         user interface for init uart
//
// PARAMETERS:          UartBautRate uart0_br - uart0 bautrate
//                      UartBautRate uart1_br - uart1 bautrate
//
// RETURN VALUE:        void
//
//======================================================================================================================
void ICACHE_FLASH_ATTR UART_Init(UartBautRate uart0_BautRate, UartBautRate uart1_BautRate)
{
    // default bautrate is 74880
    UartDev.BautRate = uart0_BautRate;
    UART_Configuration(UART_0);

    UartDev.BautRate = uart1_BautRate;
    UART_Configuration(UART_1);

    ETS_UART_INTR_ENABLE();

    // Init uart1 putc callback
    os_install_putc1((void*) UART_1_WriteCharacter);
}

//======================================================================================================================
// LOCAL FUNCTIONS
//======================================================================================================================

//======================================================================================================================
// DESCRIPTION:         Internal used function. UART0 used for data TX/RX, RX buffer size is 0x100, interrupt enabled.
//                      UART1 just used for debug output
//
// PARAMETERS:          uart_no, use UART0 or UART1 defined ahead
//
// RETURN VALUE:        void
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR UART_Configuration(uint8 uartModule)
{
    if (UART_1 == uartModule)
    {
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
    }
    else
    {
        ETS_UART_INTR_ATTACH(UART0_InterruptHandler, &(UartDev.ReceivedBuffer));
        PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
        PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
    }

    uart_div_modify(uartModule, UART_CLK_FREQ / (UartDev.BautRate));

    WRITE_PERI_REG(UART_CONF0(uartModule),
            UartDev.ExistParity | UartDev.Parity | (UartDev.StopBits << UART_STOP_BIT_NUM_S) | (UartDev.DataBits << UART_BIT_NUM_S));

    //clear rx and tx fifo,not ready
    SET_PERI_REG_MASK(UART_CONF0(uartModule), UART_RXFIFO_RST | UART_TXFIFO_RST);
    CLEAR_PERI_REG_MASK(UART_CONF0(uartModule), UART_RXFIFO_RST | UART_TXFIFO_RST);

    //set rx fifo trigger
    WRITE_PERI_REG(UART_CONF1(uartModule), (UartDev.ReceivedBuffer.TriggerLevel & UART_RXFIFO_FULL_THRHD) << UART_RXFIFO_FULL_THRHD_S);

    //clear all interrupt
    WRITE_PERI_REG(UART_INT_CLR(uartModule), 0xffff);
    //enable rx_interrupt
    SET_PERI_REG_MASK(UART_INT_ENA(uartModule), UART_RXFIFO_FULL_INT_ENA);
}

//======================================================================================================================
// DESCRIPTION:         Internal used function. Use uart1 interface to transfer one char
//                      UART1 just used for debug output
//
// PARAMETERS:          uint8 txChar - character to transmit
//
// RETURN VALUE:        OK
//
//======================================================================================================================
static STATUS ICACHE_FLASH_ATTR UART1_TransmitChar(uint8 txChar)
{
    while (true)
    {
        uint32 fifo_cnt = READ_PERI_REG(UART_STATUS(UART_1)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);
        if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) < 126)
        {
            break;
        }
    }

    WRITE_PERI_REG(UART_FIFO(UART_1), txChar);

    return OK;
}

//======================================================================================================================
// DESCRIPTION:         Do some special deal while tx char is '\r' or '\n'
//
// PARAMETERS:          char c - character to tx
//
// RETURN VALUE:        void
//
//======================================================================================================================
static void ICACHE_FLASH_ATTR UART_1_WriteCharacter(char c)
{
    if ('\n' == c)
    {
        UART1_TransmitChar('\r');
        UART1_TransmitChar('\n');
    }
    else if ('\r' == c)
    {
        UART1_TransmitChar('\r');
        UART1_TransmitChar('\n');
    }
    else
    {
        UART1_TransmitChar(c);
    }
}

//======================================================================================================================
// DESCRIPTION:         UART0 interrupt handler, add self handle code inside
//
// PARAMETERS:          void *arg - point to ETS_UART_INTR_ATTACH's arg
//
// RETURN VALUE:        void
//
//======================================================================================================================
static void UART0_InterruptHandler(void* arg)
{
    // uart0 and uart1 intr combine togther, when interrupt occur, see reg 0x3ff20020, bit2, bit0 represents
    // uart1 and uart0 respectively
    int length;
    char* string;
    uint8 receivedCharacter;
    ReceivedMessageBuffer* receivedBuffer = (ReceivedMessageBuffer *) arg;

    if (NULL == receivedBuffer)
    {
        return;
    }

    if (UART_RXFIFO_FULL_INT_ST != (READ_PERI_REG(UART_INT_ST(UART_0)) & UART_RXFIFO_FULL_INT_ST))
    {
        return;
    }

    WRITE_PERI_REG(UART_INT_CLR(UART_0), UART_RXFIFO_FULL_INT_CLR);

    while (READ_PERI_REG(UART_STATUS(UART_0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S))
    {
        receivedCharacter = READ_PERI_REG(UART_FIFO(UART_0)) & 0xFF;

        uart_tx_one_char(receivedCharacter);
        if ('\r' == receivedCharacter)
        {
            continue;
        }

        if ('\n' == receivedCharacter)
        {
            length = receivedBuffer->WritePosition - receivedBuffer->ReadPosition;

            if (length == 0)
            {
                continue;
            }

            if (length < 0)
            {
                length += RX_BUFF_SIZE;
            }

            string = (char*) os_zalloc(length + 1);

            if (NULL != string)
            {
                uint8 index;
                for (index = 0; index < length; index++)
                {
                    string[index] = UART0_ReceiveCharacter();
                }
                string[length] = '\0';

                ParseCommand(string);

                os_free(string);
            }
        }
        else
        {
            *(receivedBuffer->WritePosition) = receivedCharacter;
            receivedBuffer->WritePosition++;
        }

        // if we hit the end of the buffer, loop back to the beginning
        if (receivedBuffer->WritePosition == (receivedBuffer->ReceivedMessageBuffer + RX_BUFF_SIZE))
        {
            // overflow ...we may need more error handle here.
            receivedBuffer->WritePosition = receivedBuffer->ReceivedMessageBuffer;
        }
    }
}

//======================================================================================================================
// DESCRIPTION:         UART0 interrupt handler, add self handle code inside
//
// PARAMETERS:          void *arg - point to ETS_UART_INTR_ATTACH's arg
//
// RETURN VALUE:        void
//
//======================================================================================================================
static int ICACHE_FLASH_ATTR UART0_ReceiveCharacter()
{
    if (UartDev.ReceivedBuffer.ReadPosition == UartDev.ReceivedBuffer.WritePosition)
    {
        return -1;
    }

    int ret = *UartDev.ReceivedBuffer.ReadPosition;

    UartDev.ReceivedBuffer.ReadPosition++;

    if (UartDev.ReceivedBuffer.ReadPosition == (UartDev.ReceivedBuffer.ReceivedMessageBuffer + RX_BUFF_SIZE))
    {
        UartDev.ReceivedBuffer.ReadPosition = UartDev.ReceivedBuffer.ReceivedMessageBuffer;
    }

    return ret;
}
