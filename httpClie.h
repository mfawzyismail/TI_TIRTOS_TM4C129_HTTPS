/*
 * httpClie.h
 *
 *  Created on: May 3, 2016
 *      Author: m.fawzy
 */

#ifndef HTTPCLIE_H_
#define HTTPCLIE_H_
/* wolfSSL Header files */
#include <wolfssl/ssl.h>


int httpsClientProcess( SOCKET Sock, WOLFSSL *ssl);


#endif /* HTTPCLIE_H_ */
