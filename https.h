/*
 * https.h
 *
 *  Created on: May 3, 2016
 *      Author: m.fawzy
 */

#ifndef HTTPS_H_
#define HTTPS_H_

//#include <netmain.h>
//#include <_stack.h>
//#include "httpsif.h"

#include <sys/socket.h>
#include <wolfssl/ssl.h>
//#include <ti/ndk/inc/netmain.h>
//#include <sys/socket.h>

#define MAX_HTTP_CONNECTS       4

#define MAXREQUEST      512             /* Max bytes in request[] */

/* Structure for parsing HTTP messages */
typedef struct
{
    SOCKET Sock;                        /* Socket */
    int    flagreadall;                 /* Set if entire record read */
    int    length;                      /* Length of the parsed string */
    int    parsed;                      /* Bytes of request[] parsed (consumed) */
    int    unparsed;                    /* Bytes of request[] remaining */
    char   termstr[16];                 /* Read terminator */
    char   parsestr[16];                /* Parse delimitor */
    char   username[32];                /* Username (when AUTH tag present) */
    char   password[32];                /* Password (when AUTH tag present) */
    char   request[MAXREQUEST];         /* Request buffer */
    int    PostContentLength;
    char   URI[MAXREQUEST];             /* URI (never larger than request[]) */
    char   RequestedFile[MAXREQUEST+16];/* File (we must be able to append */
                                        /* 'index.html' to max file length) */
    char  *URIArgs;                     /* '?' arguments on URI */
    WOLFSSL *ssl;
} HTTPS_MSG;


/* Our private global functions */
int  httpsParseRecv( HTTPS_MSG *pMsg );
void httpsAuthenticationReq( SOCKET Sock, int realmIdx );

void HTTPS_vOpen();



#endif /* HTTPS_H_ */
