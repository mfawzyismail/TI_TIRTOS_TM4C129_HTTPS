/*
 * Copyright (c) 2012, Texas Instruments Incorporated
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
 * */
/*
 * ======== httpsif.h ========
 *
 */


#ifndef _HTTPIF_H_
#define _HTTPIF_H_

#include <sys/socket.h>
#include <wolfssl/ssl.h>

/* Executable function */


/* Status codes for httpsSendFullResponse() and httpsSendStatusLine() */
enum HTTP_STATUS_CODE
{
    HTTP_OK=200,
    HTTP_NO_CONTENT=204,
    HTTP_BAD_REQUEST=400,
    HTTP_AUTH_REQUIRED=401,
    HTTP_NOT_FOUND=404,
    HTTP_NOT_ALLOWED=405,
    HTTP_NOT_IMPLEMENTED=501,
    HTTP_STATUS_CODE_END
};
extern void httpsSendErrorResponse( WOLFSSL *ssl, int StatusCode );
/* Common error responses */
#define https400(ssl)  httpsSendErrorResponse((ssl), HTTP_BAD_REQUEST)
#define https404(ssl)  httpsSendErrorResponse((ssl), HTTP_NOT_FOUND)
#define https405(ssl)  httpsSendErrorResponse((ssl), HTTP_NOT_ALLOWED)
#define https501(ssl)  httpsSendErrorResponse((ssl), HTTP_NOT_IMPLEMENTED)

#endif
