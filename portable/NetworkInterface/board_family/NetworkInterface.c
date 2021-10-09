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

/* Moonranger includes */
#include "system_init.h"


/* If ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES is set to 1, then the Ethernet
 * driver will filter incoming packets and only pass the stack those packets it
 * considers need processing. */
#if ( ipconfigETHERNET_DRIVER_FILTERS_FRAME_TYPES == 0 )
    #define ipCONSIDER_FRAME_FOR_PROCESSING( pucEthernetBuffer )    eProcessBuffer
#else
    #define ipCONSIDER_FRAME_FOR_PROCESSING( pucEthernetBuffer )    eConsiderFrameForProcessing( ( pucEthernetBuffer ) )
#endif

TaskHandle_t xLanderUARTTaskHandle = NULL;

BaseType_t xNetworkInterfaceInitialise( void )
{
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
    /* FIX ME. */
    return pdFALSE;
}

void vNetworkInterfaceAllocateRAMToBuffers( NetworkBufferDescriptor_t pxNetworkBuffers[ ipconfigNUM_NETWORK_BUFFER_DESCRIPTORS ] )
{
    /* FIX ME. */
}

BaseType_t xGetPhyLinkStatus( void )
{
    /* FIX ME. */
    return pdFALSE;
}


static void prvLanderUARTHandlerTask( void * pvParameters )
{
    int retValInt = 0;
    static unsigned char readData[1];
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
			TRACE_WARNING("\n\r taskUARTtest: UART_read returned: %d for bus %d \n\r", retValInt, LANDER_COMM_UART);
		}
		printf("Read Counter Value is %d \n", (unsigned int) readData);

        // How many bytes were read
        xBytesReceived = 1;

        if(xBytesReceived > 0)
        {
            /* Allocate a network buffer descriptor that points to a buffer
            * large enough to hold the received frame.  As this is the simple
            * rather than efficient example the received data will just be copied
            * into this buffer. */
            pxBufferDescriptor = pxGetNetworkBufferWithDescriptor( xBytesReceived, 0 );

            if( pxBufferDescriptor != NULL )
            {
                // Transfer data to ethernet buffer
                for(int i=0;i<xBytesReceived;i++)
                    readData[i] = pxBufferDescriptor->pucEthernetBuffer+i;
                pxBufferDescriptor->xDataLength = xBytesReceived;
                
                // Slip decoding??


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
        else
        {
            printf("UART exited with no real data!! \n");
        }
    }

}