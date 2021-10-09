/*
 * FreeRTOS+TCP V2.3.4
 * Copyright (C) 2021 Amazon.com, Inc. or its affiliates.  All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * http://aws.amazon.com/freertos
 * http://www.FreeRTOS.org
 */

/*****************************************************************************
* Note: This file is Not! to be used as is. The purpose of this file is to provide
* a template for writing a network interface. Each network interface will have to provide
* concrete implementations of the functions in this file.
*
* See the following URL for an explanation of this file and its functions:
* https://freertos.org/FreeRTOS-Plus/FreeRTOS_Plus_TCP/Embedded_Ethernet_Porting.html
*
*****************************************************************************/

/* FreeRTOS includes. */
#include "FreeRTOS.h"
#include "list.h"

/* FreeRTOS+TCP includes */
#include "FreeRTOS_IP.h"
#include "FreeRTOS_IP_Private.h"
#include "NetworkBufferManagement.h"
#include "phyHandling.h"
#include "NetworkInterface.h"

/* Moonranger includes */
#include "system_init.h"
#include "slip.h"



/*********************************************************************/
/*                      FreeRTOS+TCP functions                       */
/*********************************************************************/

/* If ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES is set to 1, then the Ethernet
 * driver will filter incoming packets and only pass the stack those packets it
 * considers need processing. */
#if ( ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES == 0 )
    #define ipCONSIDER_FRAME_FOR_PROCESSING( pucEthernetBuffer )    eProcessBuffer
#else
    #define ipCONSIDER_FRAME_FOR_PROCESSING( pucEthernetBuffer )    eConsiderFrameForProcessing( ( pucEthernetBuffer ) )
#endif

TaskHandle_t xLanderUARTTaskHandle = NULL;
UARTtransferStatus xLanderUARTTransferStatus = done_uart;


/* 1536 bytes is more than needed, 1524 would be enough.
 * But 1536 is a multiple of 32, which gives a great alignment for cached memories. */
#define NETWORK_BUFFER_SIZE    1536 // Max packet size
static uint8_t ucBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ][ NETWORK_BUFFER_SIZE ];



BaseType_t xGetPhyLinkStatus( void )
{
    // Check transfer status
    if(xLanderUARTTransferStatus!=done_uart && xLanderUARTTransferStatus !=pending_uart)
    {
    
        printf("Lander comm transfer status in error %d \n",xLanderUARTTransferStatus);
        return pdFALSE;
    }

    // Check physical status
    UARTdriverState write_state = UART_getDriverState(LANDER_COMM_UART,write_uartDir) ;
    UARTdriverState read_state = UART_getDriverState(LANDER_COMM_UART,read_uartDir); 
    if( (write_state == uninitialized_uartState || write_state ==  error_uartState) &&
         (read_state == uninitialized_uartState || read_state ==  error_uartState))
    {
    
        printf("Lander comm link status down read: %d write %d\n",read_state,write_state);
        return pdFALSE;
    }

    return pdTRUE;
}

BaseType_t xNetworkInterfaceInitialise( void )
{
    if(xGetPhyLinkStatus()==pdFALSE)
        return pdFAIL;

    if(xLanderUARTTaskHandle == NULL)
    {
        /* Create event handler task */
        xTaskCreate( prvLanderUARTHandlerTask, /* Function that implements the task. */
                        "EMACInt",                           /* Text name for the task. */
                        256,                                 /* Stack size in words, not bytes. */
                        ( void * ) 1,                        /* Parameter passed into the task. */
                        configMAX_PRIORITIES - 1,            /* Priority at which the task is created. */
                        &xLanderUARTTaskHandle );                  /* Used to pass out the created task's handle. */
    }

    return pdPASS;
}

BaseType_t xNetworkInterfaceOutput( NetworkBufferDescriptor_t * const pxNetworkBuffer,
                                    BaseType_t xReleaseAfterSend )
{
    if( bPHYGetLinkStatus() )
    {
        // checksum ??

        // slip 
        unsigned char encodeBuffer[NETWORK_BUFFER_SIZE];
        int toSendLength = slip_encode(&encodeBuffer[0],pxNetworkBuffer->xDataLength,pxNetworkBuffer->pucEthernetBuffer);

        // Async write
        UARTgenericTransfer transmit_transfer = {.bus = LANDER_COMM_UART, 
                                                .direction = write_uartDir,
                                                .writeData = pxNetworkBuffer->pucEthernetBuffer,
                                                .writeSize = toSendLength,
                                                .result = &xLanderUARTTransferStatus};
        UART_queueTransfer(&transmit_transfer);

        /* Call the standard trace macro to log the send event. */
        iptraceNETWORK_INTERFACE_TRANSMIT();
    }


    if( xReleaseAfterSend != pdFALSE )
    {
        /* It is assumed SendData() copies the data out of the FreeRTOS+TCP Ethernet
         * buffer.  The Ethernet buffer is therefore no longer needed, and must be
         * freed for re-use. */
        vReleaseNetworkBufferAndDescriptor( pxNetworkBuffer );
    }

    return pdTRUE;
}

void vNetworkInterfaceAllocateRAMToBuffers( NetworkBufferDescriptor_t pxNetworkBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ] )
{
    BaseType_t x;

    for( x = 0; x < ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS; x++ )
    {
        /* pucEthernetBuffer is set to point ipBUFFER_PADDING bytes in from the
            * beginning of the allocated buffer. */
        pxNetworkBuffers[ x ].pucEthernetBuffer = &( ucBuffers[ x ][ ipBUFFER_PADDING ] );

        /* The following line is also required, but will not be required in
            * future versions. */
        *( ( uint32_t * ) &ucBuffers[ x ][ 0 ] ) = ( uint32_t ) &( pxNetworkBuffers[ x ] );
    }
}

static void prvLanderUARTHandlerTask( void * pvParameters )
{
    int retValInt = 0;
    static unsigned char readData;
    static unsigned char decodeBuffer[NETWORK_BUFFER_SIZE];
    NetworkBufferDescriptor_t * pxBufferDescriptor;
    size_t xBytesReceived = 0, xBytesRead = 0;

    uint16_t xICMPChecksumResult = ipCORRECT_CRC;
    const IPPacket_t * pxIPPacket;


    /* Used to indicate that xSendEventStructToIPTask() is being called because
     * of an Ethernet receive event. */
    IPStackEvent_t xRxEvent;

    for (; ; )
    {
        /* Wait for new data on UART */
        retValInt = UART_read(LANDER_COMM_UART, &readData, 1);
		if(retValInt != 0) {
			TRACE_WARNING("\n\r prvLanderUARTHandlerTask: UART_read returned: %d for bus %d \n\r", retValInt, LANDER_COMM_UART);
            printf("\n\r prvLanderUARTHandlerTask: UART_read returned: %d for bus %d \n\r", retValInt, LANDER_COMM_UART);
		}
		
        // check if read buffer is full
        if(xBytesReceived+1>NETWORK_BUFFER_SIZE)
        {
            xBytesReceived = 0;
            printf("\n\r prvLanderUARTHandlerTask: Read buffer full!! Resetting buffer ");
        }
        decodeBuffer[xBytesReceived++] = readData;


        // Check for end of packet or wait for the end
        if(readData == SLIP_END && xBytesReceived>1)
        {
            /* Allocate a network buffer descriptor that points to a buffer
            * large enough to hold the received frame.  As this is the simple
            * rather than efficient example the received data will just be copied
            * into this buffer. */
            pxBufferDescriptor = pxGetNetworkBufferWithDescriptor( xBytesReceived, 0 );

            if( pxBufferDescriptor != NULL )
            {
                // Transfer data to ethernet buffer
                pxBufferDescriptor->xDataLength = slip_read_packet(&decodeBuffer[0], pxBufferDescriptor->pucEthernetBuffer,xBytesReceived);

                // Clear decode buffer after decoding
                xBytesReceived = 0;

                // Checksum ??

                if( ( ipCONSIDER_FRAME_FOR_PROCESSING( pxBufferDescriptor->pucEthernetBuffer ) == eProcessBuffer ) &&
                    ( xICMPChecksumResult == ipCORRECT_CRC ) )
                {
                    /* The event about to be sent to the TCP/IP is an Rx event. */
                    xRxEvent.eEventType = eNetworkRxEvent;

                    /* pvData is used to point to the network buffer descriptor that
                     * now references the received data. */
                    xRxEvent.pvData = ( void * ) pxBufferDescriptor;

                    /* Send the data to the TCP/IP stack. */
                    if( xSendEventStructToIPTask( &xRxEvent, 0 ) == pdFALSE )
                    {
                        /* The buffer could not be sent to the IP task so the buffer
                         * must be released. */
                        vReleaseNetworkBufferAndDescriptor( pxBufferDescriptor );

                        /* Make a call to the standard trace macro to log the
                         * occurrence. */
                        iptraceETHERNET_RX_EVENT_LOST();
                    }
                    else
                    {
                        /* The message was successfully sent to the TCP/IP stack.
                        * Call the standard trace macro to log the occurrence. */
                        iptraceNETWORK_INTERFACE_RECEIVE();
                    }
                }
                else
                {
                    /* The Ethernet frame can be dropped, but the Ethernet buffer
                     * must be released. */
                    vReleaseNetworkBufferAndDescriptor( pxBufferDescriptor );
                }

            }
            else
            {
                /* The event was lost because a network buffer was not available.
                * Call the standard trace macro to log the occurrence. */
                iptraceETHERNET_RX_EVENT_LOST();
            }
        }
    }

}