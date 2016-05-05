/*
 * Copyright (c) 2014-2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *    ======== main.c ========
 */

/*
 * This is modified version of the tcpecho using the TLS to provide the https server example
 * Author: Mohammed Fawzy @ 5/5/2016
 */


#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/hal/Seconds.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/clock.h>
#include <ti/drivers/GPIO.h>
#include "index_withCGI.h"
#include "greetings.h"
#include "chip.h"
#include "efss.h"
#include "Globalhttps.h"

Int getTime(WOLFSSL *ssl, int length)
{
    Char buf[200];
    static UInt scalar = 0;

    if (scalar == 0)
    {
        scalar = 1000000u / Clock_tickPeriod;
    }
    httpsSendStatusLine(ssl, HTTP_OK, CONTENT_TYPE_HTML);
    httpsSendClientStr(ssl, CRLF);
    httpsSendClientStr(ssl,
        "<html><head><title>SYS/BIOS Clock "\
    "Time</title></head><body><h1>Time</h1>\n");
    System_sprintf(buf, "<p>Up for %d seconds</p>\n",
        ((unsigned long)Clock_getTicks() / scalar));
    httpsSendClientStr(ssl, buf);
    httpsSendClientStr(ssl, "</table></body></html>");

    return (1);
}
Void AddWebFiles(Void)
{
    //Note: both INDEX_SIZE and INDEX are defined in index.h
    efss_createfile("index.html", INDEX_SIZE, (unsigned char *)INDEX);
    efss_createfile("getTime.cgi", 0, (unsigned char *)&getTime);
    efss_createfile("greetings.html", GREETINGS_SIZE, (unsigned char *)GREETINGS);
    efss_createfile("chip.jpg", CHIP_SIZE, (unsigned char *)CHIP);
}

Void RemoveWebFiles(Void)
{
    efss_destroyfile("index.html");
    efss_destroyfile("getTime.cgi");
    efss_destroyfile("greetings.html");
    efss_destroyfile("chip.jpg");
}

/* Example/Board Header file */
#include "Board.h"
/*
 *  ======== main ========
 */
int main(void)
{
    /* Call board init functions */
    Board_initGeneral();
    Board_initGPIO();
    Board_initEMAC();


    System_printf("Starting the TLS/TCP Echo example\nSystem provider is set "
            "to SysMin. Halt the target to view any SysMin contents in"
            " ROV.\n");
    /* SysMin will only print to the console when you call flush or exit */
    System_flush();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
