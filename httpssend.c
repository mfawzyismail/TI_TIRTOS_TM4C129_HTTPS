/*
 * Copyright (c) 2012-2014, Texas Instruments Incorporated
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
 * ======== httpsend.c ========
 *
 * Send functions used by HTTP server and applicaitons
 *
 */

//#include "https.h"
#include <string.h>
#include "httpstr.h"
#include "httpsif.h"
//#include "httpsif.h"
#include <sys/socket.h>
#include <wolfssl/ssl.h>
#include "efss.h"
//#include <ti/ndk/inc/netmain.h>
//#include <ti/ndk/inc/usertype.h>
//#include <ti/ndk/inc/os/osif.h>

//#include "osif.h"
#define MAX_RESPONSE_SIZE  200

#define MY_DEBUG 0
#define NDK_HTTP_DATABUFSIZE 80



static void SetContentType(char *ContentType, char *FileName);
static void httpsSendErrorHTML( WOLFSSL *ssl, int StatusCode );
void httpsSendEntityLength(WOLFSSL *ssl, int EntityLength);
void httpsSendClientStr(WOLFSSL *ssl, char *Response);

typedef struct _codestr {
    int  code;
    char *string;
} CODESTR;
#define EFS_FILE            void
static CODESTR codestr[] = {
    { HTTP_OK, " OK" },
    { HTTP_NO_CONTENT, " No Content" },
    { HTTP_BAD_REQUEST, " Bad Request" },
    { HTTP_AUTH_REQUIRED, " Authorization Required" },
    { HTTP_NOT_FOUND, " File Not Found" },
    { HTTP_NOT_IMPLEMENTED, " Not Implemented" },
    { HTTP_NOT_ALLOWED, " Not Allowed" },
    { 0, " Unknown" } };

int (*httpsErrorResponseHook)(SOCKET Sock, int StatusCode) = 0;


/*
 * httpsSendErrorHTML
 *
 * Sends the HTML portion of an error message, consisting of the
 * "Content Length" tag and the HTLM code itself.
 *
 * Used to generate a standard HTML page for error messages when
 * the application code does not provide one of its own.
 */
extern int  NDK_sprintf(char *s, const char *format, ...);
static void httpsSendErrorHTML(  WOLFSSL *ssl, int StatusCode )
{
    char Data[NDK_HTTP_DATABUFSIZE];
    char *footer = "</h1></body></html>";
    int  i;
    int sizeRemaining = NDK_HTTP_DATABUFSIZE;

    /* Build the HTML response */
    sizeRemaining = sizeRemaining -
            NDK_sprintf(Data, "<html><body><h1>HTTP/1.0 %3d -", StatusCode);

    for (i = 0; codestr[i].code && codestr[i].code != StatusCode; i++);

    /* ensure there's room available in Data to write the code string */
    if (strlen(codestr[i].string) < sizeRemaining) {
        strcat(Data, codestr[i].string);
        sizeRemaining = sizeRemaining - strlen(codestr[i].string);
    }
    else {
        DbgPrintf(DBG_INFO, "httpsSendErrorHTML: not enough space in array to build HTML response (1)\n");
    }

    /* ensure there's room available in Data to write the footer string */
    if (strlen(footer) < sizeRemaining) {
        strcat(Data, footer);
        sizeRemaining = sizeRemaining - strlen(footer);
    }
    else {
        DbgPrintf(DBG_INFO, "httpsSendErrorHTML: not enough space in array to build HTML response (2)\n");
    }

    /* Send the length of the HTML response */
    httpsSendEntityLength(ssl, strlen(Data));

    /* Send the respone data */
    httpsSendClientStr( ssl, Data );
}


/*
 * SetContentType
 *
 * Fills in the supplied string with a content type string that
 * matches the supplied filename.
 */
static void SetContentType(char *ContentType, char *FileName)
{
   char *psExt = FileName;

   /*  psExt will point at the ".xxx" part of filename */
   /*  If FileName does not have an extension it will point ot terminating char */
   while( *psExt && *psExt != '.' )
       psExt++;

   if     ( !strcmp(".au",    psExt) ) strcpy(ContentType, CONTENT_TYPE_AU);
   else if( !strcmp(".class", psExt) ) strcpy(ContentType, CONTENT_TYPE_APPLET);
   else if( !strcmp(".css", psExt) ) strcpy(ContentType,   CONTENT_TYPE_CSS);
   else if( !strcmp(".doc",   psExt) ) strcpy(ContentType, CONTENT_TYPE_DOC);
   else if( !strcmp(".gif",   psExt) ) strcpy(ContentType, CONTENT_TYPE_GIF);
   else if( !strcmp(".htm",   psExt) ) strcpy(ContentType, CONTENT_TYPE_HTML);
   else if( !strcmp(".html",  psExt) ) strcpy(ContentType, CONTENT_TYPE_HTML);
   else if( !strcmp(".jpg",   psExt) ) strcpy(ContentType, CONTENT_TYPE_JPG);
   else if( !strcmp(".mpg",   psExt) ) strcpy(ContentType, CONTENT_TYPE_MPEG);
   else if( !strcmp(".mpeg",  psExt) ) strcpy(ContentType, CONTENT_TYPE_MPEG);
   else if( !strcmp(".pdf",   psExt) ) strcpy(ContentType, CONTENT_TYPE_PDF);
   else if( !strcmp(".wav",   psExt) ) strcpy(ContentType, CONTENT_TYPE_WAV);
   else if( !strcmp(".zip",   psExt) ) strcpy(ContentType, CONTENT_TYPE_ZIP);
   else strcpy(ContentType, CONTENT_TYPE_APPLET);
}


/*
 * httpsSendEntityLength
 * Write out the entiry length tag, and terminates the header
 * with an additional CRLF.
 *
 * Since the header is terminated, this must be the last tag
 * written. Entity data should follow immediately.
 */
void httpsSendEntityLength(WOLFSSL *ssl, int EntityLength)
{
    char Response[MAX_RESPONSE_SIZE];

    if( EntityLength >= 0 )
    {
        NDK_sprintf(Response, "%s%d\r\n\r\n", CONTENT_LENGTH, EntityLength);
        httpsSendClientStr( ssl, Response );
    }
}


/*
 * httpsSendClientStr
 *
 * Sends the indicated NULL terminated response string to
 * the indicated client socket - i.e.: calls strlen() and send()
 */
void httpsSendClientStr(WOLFSSL *ssl, char *Response)
{
    wolfSSL_send(ssl, Response, strlen(Response), 0);
}


/* 
 * httpsSendStatusLine
 *
 * Sends a formatted response message to Sock with the given
 * status code and content type. The value of ContentType can
 * be NULL if no ContentType is required.
 */
void httpsSendStatusLine(WOLFSSL *ssl, int StatusCode, char *ContentType)
{
    char Response[MAX_RESPONSE_SIZE];
    int  i;
    int sizeRemaining = MAX_RESPONSE_SIZE;

    /* Bound status code so that a very large code can't crash us */
    if (StatusCode < 0 || StatusCode > 999) {
        StatusCode = 999;
    }

    /*
     * sizeRemaing will track available space in string. Its value includes the
     * '\0' null terminating character.
     */
    sizeRemaining = sizeRemaining -
            NDK_sprintf(Response, "%s%3d", HTTP_VER, StatusCode);

    for (i = 0; codestr[i].code && codestr[i].code != StatusCode; i++);

    if (strlen(codestr[i].string) < sizeRemaining) {
        strcat(Response, codestr[i].string);
        sizeRemaining = sizeRemaining - strlen(codestr[i].string);
    }
    else {
        DbgPrintf(DBG_INFO, "httpsSendStatusLine: not enough space in Response array to build HTML string (1)\n");
    }

    /* ensure there's room available in Response[] to write the code string */
    if (strlen(CRLF) < sizeRemaining) {
        strcat(Response, CRLF);
        sizeRemaining = sizeRemaining - strlen(CRLF);
    }
    else {
        DbgPrintf(DBG_INFO, "httpsSendStatusLine: not enough space in Response array to build HTML string (2)\n");
    }

    if( ContentType )
    {
        /* ensure there's room available in Response[] to write the code string */
        if (strlen(CONTENT_TYPE) < sizeRemaining) {
            strcat(Response, CONTENT_TYPE);
            sizeRemaining = sizeRemaining - strlen(CONTENT_TYPE);
        }
        else {
            DbgPrintf(DBG_INFO, "httpsSendStatusLine: not enough space in Response array to build HTML string (3)\n");
        }

        /* ensure there's room available in Response[] to write the code string */
        if (strlen(ContentType) < sizeRemaining) {
            strcat(Response, ContentType);
            sizeRemaining = sizeRemaining - strlen(ContentType);
        }
        else {
            DbgPrintf(DBG_INFO, "httpsSendStatusLine: not enough space in Response array to build HTML string (4)\n");
        }

        /* ensure there's room available in Response[] to write the code string */
        if (strlen(CRLF) < sizeRemaining) {
            strcat(Response, CRLF);
            sizeRemaining = sizeRemaining - strlen(CRLF);
        }
        else {
            DbgPrintf(DBG_INFO, "httpsSendStatusLine: not enough space in Response array to build HTML string (5)\n");
        }
    }
    httpsSendClientStr( ssl, Response );
}


/*
 * httpsSendFullResponse
 *
 * Sends a full formatted response message to Sock, including the
 * file indicated by the filename in RequestedFile. The status code
 * for this call is usually HTTP_OK.
 */
void httpsSendFullResponse(WOLFSSL *ssl, int StatusCode, char *RequestedFile)
{
    EFS_FILE *fEntityBody = 0;
    char  tempBuffer[NDK_HTTP_DATABUFSIZE];
    int EntityLength;

    /* Open the file */
    fEntityBody = efss_fopen( RequestedFile, "rb" );

    /* If either failed, report file not found */
    if( !fEntityBody )
    {
        https404(ssl);   /* file not found */
        goto EXIT;
    }

    /* Send the HTTP status line */
    SetContentType(tempBuffer, RequestedFile);
    httpsSendStatusLine(ssl, StatusCode, tempBuffer);

    /* Send the Entity Length */
    EntityLength = efss_getfilesize( fEntityBody );
    httpsSendEntityLength(ssl, EntityLength);

    /* We have EFS send the file since it can copy the raw file */
    /* data directly to the socket (no intermediate copy) */
    efss_filesend( fEntityBody, EntityLength, ssl );

EXIT:
    if( fEntityBody )
        efss_fclose( fEntityBody );
    return;
}


/*
 * httpsSendErrorResponse
 *
 * Sends a formatted error response message to Sock based on the
 * status code.
 */
void httpsSendErrorResponse( WOLFSSL *ssl, int StatusCode )
{
    /* Bound status code so that a very large code can't crash us */
    if( StatusCode < 0 || StatusCode > 999 )
        StatusCode = 999;

    /* Send the HTTP status code */
    httpsSendStatusLine(ssl, StatusCode, CONTENT_TYPE_HTML);

    /* Send a default HTML response if there is no user callback */
    if( !httpsErrorResponseHook || !httpsErrorResponseHook( ssl, StatusCode ) )
        httpsSendErrorHTML( ssl, StatusCode );
}

/*
 * httpsAuthenticationReq
 *
 * Sends a formatted error response message to Sock requesting authentication
 * on the supplied realm index.
 */
void httpsAuthenticationReq( WOLFSSL *ssl, int realmIdx )
{
#if 0
    char    tmpbuffer[NDK_HTTP_DATABUFSIZE];
    char    tmpname[32];
    HANDLE  hName;
    int     rc;
    int     size;

    /* Send the status line */
    httpsSendStatusLine(Sock, 401, CONTENT_TYPE_HTML);

    /* Send the authenticate tag */
    /* (Try to get the authentication group name from the registry) */
    rc = CfgGetEntry( 0, CFGTAG_SYSINFO,
                      CFGITEM_SYSINFO_REALM1+realmIdx-1,
                      1, &hName );
    if( rc > 0 )
    {
        size = 31;
        rc = CfgEntryGetData( hName, &size, (unsigned char *)tmpname );
        CfgEntryDeRef( hName );
        if( rc > 0 )
            tmpname[rc]=0;
    }

    if( rc <= 0 )
        NDK_sprintf( tmpname, "DefaultRealm%d", realmIdx );

    NDK_sprintf( tmpbuffer, "WWW-Authenticate: Basic realm=\"%s\"\r\n", tmpname );
    httpsSendClientStr(Sock, tmpbuffer);

    /* Send a default HTML response if there is no user callback */
    if( !httpsErrorResponseHook || !httpsErrorResponseHook( Sock, 401 ) )
        httpsSendErrorHTML( Sock, 401 );
#endif
}

