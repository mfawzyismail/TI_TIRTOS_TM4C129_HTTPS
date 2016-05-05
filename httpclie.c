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
 * ======== httpsclie.c ========
 *
 */

//#include "https.h"
#include <stdlib.h>
#include <string.h>
//#include "httpsif.h"
/* NDK Header files */
//#include <sys/socket.h>
#include "https.h"
#include "httpstr.h"
#include "httpsif.h"
#include "httpssend.h"
#include "efss.h"
/* Operating System */
//#include "ti/ndk/inc/os/osif.h"

/* Possible return flags for efss_filecheck() */
#define EFS_FC_NOTFOUND         0x01        /* File not found */
#define EFS_FC_NOTALLOWED       0x02        /* File can not be accessed */
#define EFS_FC_EXECUTE          0x04        /* Filename is a function call (CGI) */
#define EFS_FC_AUTHFAILED       0x08        /* File autentication failed */
                                            /* (realm index suppied in realm) */

/* This is a pretty simple HTTP server. */
/* Here are the tags we look for... */
#define TAG_GET         1
#define TAG_POST        2
#define TAG_CLEN        3
#define TAG_AUTH        4
#define TAG_DONTCARE    5

/* Here is were we check for them... */

static int httpsExtractTag( char *pTagStr )
{
    if( !strncmp("GET", pTagStr, 3) )
        return( TAG_GET );
    if( !strncmp("POST", pTagStr, 4) )
        return( TAG_POST );
    if( !strncmp("Content-Length: ", pTagStr, 16) )
        return( TAG_CLEN );
    if( !strncmp("Content-length: ", pTagStr, 16) )
        return( TAG_CLEN );
    if( !strncmp("Authorization: Basic ", pTagStr, 21) )
        return( TAG_AUTH );

    return( TAG_DONTCARE );
}

static int httpsGetMimeBits( unsigned char c, unsigned char *pb )
{
    if( c>='A' && c<='Z' )
        *pb = (c - 'A') + 0;
    else if( c>='a' && c<='z' )
        *pb = (c - 'a') + 26;
    else if( c>='0' && c<='9' )
        *pb = (c - '0') + 52;
    else if( c == '+' )
        *pb = 62;
    else if( c == '/' )
        *pb = 63;
    else if( c == '=' )
        *pb = 0;
    else
        return(0);
    return(1);
}

static void httpsGetAuthParams( HTTPS_MSG *pMsg, unsigned char *pMimeStr )
{
    unsigned char b1,b2,b3,b4;
    unsigned char tempstr[68];
    int   i,j,ok=1;

    i=0;
    while( i<64 )
    {
        ok &= httpsGetMimeBits( *pMimeStr++, &b1 );
        ok &= httpsGetMimeBits( *pMimeStr++, &b2 );
        ok &= httpsGetMimeBits( *pMimeStr++, &b3 );
        ok &= httpsGetMimeBits( *pMimeStr++, &b4 );
        if( !ok )
            break;
        b1 <<= 2;
        b1 |= (b2 >> 4 );
        b2 <<= 4;
        b2 |= (b3 >> 2 );
        b3 <<= 6;
        b3 |= b4;

        tempstr[i++] = b1;
        tempstr[i++] = b2;
        tempstr[i++] = b3;
    }
    tempstr[i] = 0;

    j=0;
    /* Username */
    i=0;
    while( tempstr[j] && tempstr[j] != ':' && i<31 )
        pMsg->username[i++] = tempstr[j++];
    pMsg->username[i] = 0;
    if( tempstr[j] == ':' )
        j++;

    /* Password */
    i=0;
    while( tempstr[j] && i<31 )
        pMsg->password[i++] = tempstr[j++];
    pMsg->password[i] = 0;
}


/*------------------------------------------------------------------------- */
/* httpsClientProcess() */
/* This is the entrypoint for all HTTP requests. */
/* Returns: */
/*    1 Socket still open */
/*    0 Socket closed */
/*------------------------------------------------------------------------- */
int httpsClientProcess( SOCKET Sock, WOLFSSL *ssl)
{
    HTTPS_MSG *pMsg = 0;
    int      nMethod;
    int      rc = 0;
    int      length,i,realmidx;
    int      (*PostFunction)(WOLFSSL *, int, unsigned char *) = 0;
    int      fCGI = 0;
    int termstrSizeRemaining = sizeof(pMsg->termstr);
    int parseSizeRemaining   = sizeof(pMsg->parsestr);
    int requestSizeRemaining = sizeof(pMsg->RequestedFile);
    struct timeval to;
    struct linger  lgr;


    /* Init the socket parameters */

    lgr.l_onoff  = 1;
    lgr.l_linger = 5;
    rc = setsockopt((int)Sock, SOL_SOCKET, SO_LINGER, &lgr, sizeof( lgr ));
    if (rc < 0) {
        goto SHUTDOWN;
    }

    /* Configure our socket timeout to be 10 seconds */
    to.tv_sec  = 20;
    to.tv_usec = 0;
    rc = setsockopt( (int)Sock, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof( to ) );
    if (rc < 0) {
        goto SHUTDOWN;
    }

    rc = setsockopt( (int)Sock, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof( to ) );
    if (rc < 0) {
        goto SHUTDOWN;
    }

    /* Start the HTTP message processing */

    pMsg = mmAlloc( sizeof(HTTPS_MSG) );
    if( !pMsg )
        goto SHUTDOWN;

    /* Init the message record */
    pMsg->Sock              = Sock;
    pMsg->ssl               = ssl;
    pMsg->parsed            = 0;
    pMsg->unparsed          = 0;
    pMsg->flagreadall       = 0;
    pMsg->length            = 0;
    pMsg->PostContentLength = 0;
    pMsg->URIArgs           = 0;
    pMsg->username[0]       = 0;
    pMsg->password[0]       = 0;

    if (strlen(CRLF) < termstrSizeRemaining) {
        strcpy(pMsg->termstr,  CRLF );
        termstrSizeRemaining = termstrSizeRemaining - strlen(CRLF);
    }
    else {
        /* Error constructing message, return failure */
        goto SHUTDOWN;
    }

    if (strlen(CRLF) < termstrSizeRemaining) {
        strcat(pMsg->termstr,  CRLF );
        termstrSizeRemaining = termstrSizeRemaining - strlen(CRLF);
    }
    else {
        /* Error constructing message, return failure */
        goto SHUTDOWN;
    }

    if (strlen(SPACE) < parseSizeRemaining) {
        strcpy(pMsg->parsestr, SPACE);
        parseSizeRemaining = parseSizeRemaining - strlen(SPACE);
    }
    else {
        /* Error constructing message, return failure */
        goto SHUTDOWN;
    }

    /* Read the method */
    rc = httpsParseRecv( pMsg );
    if( rc<=0  )
    {
        https400(ssl);
        goto SHUTDOWN;
    }
    nMethod = httpsExtractTag( pMsg->request );

    /* Get URI */
    rc = httpsParseRecv( pMsg );
    if( rc<=0 )
    {
        https400(ssl);
        goto SHUTDOWN;
    }
    strcpy( pMsg->URI, pMsg->request );

    /* Setup for CRLF delimitor (CRLFCRLF is still terminator) */
    if (strlen(CRLF) < parseSizeRemaining) {
        strcpy(pMsg->parsestr, CRLF);
        parseSizeRemaining = parseSizeRemaining - strlen(CRLF);
    }
    else {
        /* Error constructing message, return failure */
        goto SHUTDOWN;
    }

    /* Scan all tags for tags we're interested in */
    for(i=0;;i=1)
    {
        int nTag;

        rc = httpsParseRecv( pMsg );
        if( rc < 0 )
        {
            https400(ssl);
            goto SHUTDOWN;
        }
        if( rc == 0 )
            break;
        /* A null tag after a tag is a CRLFCRLF */
        if( i && !pMsg->request[0] )
            break;

        /* Extract the tag */
        nTag = httpsExtractTag( pMsg->request );

        /* Process all the tag types we care about */
        if( nTag == TAG_CLEN )
            pMsg->PostContentLength = atoi(pMsg->request + 16);

        if( nTag == TAG_AUTH )
            httpsGetAuthParams( pMsg, (unsigned char *)pMsg->request + 21 );
    }

    /* Process the URI into RequestedFile */
    length = strlen(pMsg->URI);

    /* Scan URI for a '?' and terminate at that point */
    for(i=0;i<length;i++)
        if( pMsg->URI[i] == '?' )
        {
            pMsg->URI[i] = 0;
            pMsg->URIArgs = pMsg->URI + i + 1;
            break;
        }

    /* Set the new string length (if any) */
    length = i;

    /* Assume success from this point on */
    rc = 1;

    /* If URI terminates with a '/', then add index.html, UNLESS */
    /* length is 1, in which case replace '/' with index.html */
    /* If URI does not terminate with a '/', use it as is */
    /* Note: We always remove the leading '/' */
    if( !length || (length == 1 && pMsg->URI[0] == '/') ) {
        if (strlen(DEFAULT_NAME) < requestSizeRemaining) {
            strcpy(pMsg->RequestedFile, DEFAULT_NAME);
            requestSizeRemaining =
                    requestSizeRemaining - strlen(DEFAULT_NAME);
        }
        else {
            /* Error constructing message, return failure */
            goto SHUTDOWN;
        }
    }
    else {
        if (strlen(pMsg->URI + 1) < requestSizeRemaining) {
            strcpy(pMsg->RequestedFile, pMsg->URI + 1);
            requestSizeRemaining =
                    requestSizeRemaining - strlen(pMsg->URI + 1);

            if (pMsg->URI[length - 1] == '/') {
                if (strlen(DEFAULT_NAME) < requestSizeRemaining) {
                    strcat(pMsg->RequestedFile, DEFAULT_NAME);
                    requestSizeRemaining =
                            requestSizeRemaining - strlen(DEFAULT_NAME);
                }
                else {
                    /* Error constructing message, return failure */
                    goto SHUTDOWN;
                }
            }
        }
        else {
            /* Error constructing message, return failure */
            goto SHUTDOWN;
        }
    }

    /* Authenticate the request */
    i=efss_filecheck(pMsg->RequestedFile,pMsg->username,pMsg->password,&realmidx);

    /* Act on filecheck status code */
    if( i & EFS_FC_NOTFOUND )
    {
        https404(ssl);
        goto SHUTDOWN;
    }
    if( i & EFS_FC_AUTHFAILED )
    {
        httpsAuthenticationReq( ssl, realmidx );
        goto SHUTDOWN;
    }
    if( i & EFS_FC_NOTALLOWED )
    {
        https405(ssl);
        goto SHUTDOWN;
    }
    if( i & EFS_FC_EXECUTE )
    {
        fCGI = 1;
        PostFunction = (int(*)(WOLFSSL *, int, unsigned char *))
                                   efss_loadfunction(pMsg->RequestedFile);
    }

    /* Execute based on the method and fCGI flag */
    switch (nMethod)
    {
    case TAG_GET:
        if( !fCGI ) {
            httpsSendFullResponse( ssl, HTTP_OK, pMsg->RequestedFile );
        }
        else
        {
            if( !PostFunction ) {  	
                https404(ssl);   /* function does not exist */
            }
            else
            {
                /* Now call Post function using the "get" syntax */
                if( !PostFunction( ssl, 0, (unsigned char *)pMsg->URIArgs ) )
                {
                    rc = 0;
                    goto SHUTDOWN_postclose;
                }
            }
        }
        break;

    case TAG_POST:
        if( !fCGI )
            https405(ssl);   /* file is not a CGI */
        else if( !PostFunction )
            https404(ssl);   /* function does not exist */
        /* Now call Post function using "post" syntax */
        else if( !PostFunction( ssl, pMsg->PostContentLength,
                               (unsigned char *)pMsg->URIArgs ) )
        {
            rc = 0;
            goto SHUTDOWN_postclose;
        }
        break;

    default:
        https501( ssl );   /*  Not Implemented */
        break;
    }

SHUTDOWN:
#if 0
/* Close socket on error */
    if( rc <= 0 )
        fdClose( Sock );
#endif
SHUTDOWN_postclose:
    /* Free buffer if we allocated one */
    if( pMsg )
        mmFree( pMsg );

    if( rc < 0 )
        rc = 0;

    return( rc );
}
