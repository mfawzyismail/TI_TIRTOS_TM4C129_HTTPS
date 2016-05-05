/*
 * httpssend.h
 *
 *  Created on: May 3, 2016
 *      Author: m.fawzy
 */

#ifndef HTTPSSEND_H_
#define HTTPSSEND_H_

void httpsSendFullResponse(WOLFSSL *ssl, int StatusCode, char *RequestedFile);
void httpsSendStatusLine(WOLFSSL *ssl, int StatusCode, char *ContentType);
void httpsSendStatusLine(WOLFSSL *ssl, int StatusCode, char *ContentType);
void httpsSendClientStr(WOLFSSL *ssl, char *Response);


#endif /* HTTPSSEND_H_ */
