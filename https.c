/*
 * https.c
 *
 *  Created on: May 3, 2016
 *      Author: m.fawzy
 */
#include <string.h>

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>


/* wolfSSL Header files */
#include <wolfssl/ssl.h>
#include <wolfssl/certs_test.h>
#include "https.h"

#include <sys/socket.h>
//#include <ti/ndk/inc/netmain.h>
#include "httpClie.h"



#define HTTPS_PORT 443

#ifdef WOLFSSL_TIRTOS
#define HTTPSHANDLERSTACK 8704
#else
#define HTTPSHANDLERSTACK 1024
#endif

#define HttpClientSTACKSIZE 32768
#define TCPPACKETSIZE 256
#define NUMHttpClientS 2

/* Prototypes */
Void HttpHandler(UArg arg0, UArg arg1);

/* USER STEP: update to current time (in secs) since epoch (i.e. Jan 1, 1970) */
/* http://www.epochconverter.com/ */
#define CURRENTTIME 1461848656

/*
 *  ======== exitApp ========
 *  Cleans up the SSL context and exits the application
 */
Void exitApp(WOLFSSL_CTX *ctx)
{
    if(ctx != NULL)
    {
        wolfSSL_CTX_free(ctx);
        wolfSSL_Cleanup();
    }

    BIOS_exit(-1);
}
extern Void AddWebFiles(Void);

/*
 *  ======== HttpClient ========
 *  Task to handle TCP connection. Can be multiple Tasks running
 *  this function.
 */
Void HttpClient(UArg arg0, UArg arg1)
{
    int clientfd = 0;
    WOLFSSL *ssl = (WOLFSSL *) arg0;
    SOCKET     HTTPSSocket = (SOCKET)arg1;


    clientfd = wolfSSL_get_fd(ssl);
    System_printf("HttpClient: start clientfd = 0x%x\n", clientfd);
    System_flush();
    httpsClientProcess(HTTPSSocket, ssl);
    System_printf("HttpClient: stop clientfd = 0x%x\n", clientfd);
    System_flush();
    wolfSSL_free(ssl);
    close(clientfd);
}

/*
 *  ======== tcpHandler ========
 *  Creates new Task to handle new TCP connections.
 */
Void HttpHandler(UArg arg0, UArg arg1)
{
    int status;
    int clientfd;
    int server;
    struct sockaddr_in localAddr;
    struct sockaddr_in clientAddr;
    int optval;
    int optlen = sizeof(optval);
    socklen_t addrlen = sizeof(clientAddr);
    Task_Handle taskHandle;
    Task_Params taskParams;
    Error_Block eb;
    WOLFSSL_CTX *ctx;
    WOLFSSL *ssl;

    wolfSSL_Init();

    ctx = wolfSSL_CTX_new(wolfTLSv1_2_server_method());
    if(ctx == NULL)
    {
        System_printf("tcpHandler: wolfSSL_CTX_new error.\n");
        goto shutdown;
    }

    if(wolfSSL_CTX_load_verify_buffer(ctx, ca_cert_der_2048, sizeof(ca_cert_der_2048) / sizeof(char), SSL_FILETYPE_ASN1)
            != SSL_SUCCESS)
    {
        System_printf("tcpHandler: Error loading ca_cert_der_2048"
                " please check the wolfssl/certs_test.h file.\n");
        goto shutdown;
    }

    if(wolfSSL_CTX_use_certificate_buffer(ctx, server_cert_der_2048, sizeof(server_cert_der_2048) / sizeof(char),
            SSL_FILETYPE_ASN1) != SSL_SUCCESS)
    {
        System_printf("tcpHandler: Error loading server_cert_der_2048,"
                " please check the wolfssl/certs_test.h file.\n");
        goto shutdown;
    }

    if(wolfSSL_CTX_use_PrivateKey_buffer(ctx, server_key_der_2048, sizeof(server_key_der_2048) / sizeof(char),
            SSL_FILETYPE_ASN1) != SSL_SUCCESS)
    {
        System_printf("tcpHandler: Error loading server_key_der_2048,"
                " please check the wolfssl/certs_test.h file.\n");
        goto shutdown;
    }

    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server == -1)
    {
        System_printf("tcpHandler: socket not created.\n");
        goto shutdown;
    }

    memset(&localAddr, 0, sizeof(localAddr));
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(arg0);

    status = bind(server, (struct sockaddr *) &localAddr, sizeof(localAddr));
    if(status == -1)
    {
        System_printf("tcpHandler: bind failed.\n");
        goto shutdown;
    }

    status = listen(server, NUMHttpClientS);
    if(status == -1)
    {
        System_printf("tcpHandler: listen failed.\n");
        goto shutdown;
    }

    if(setsockopt(server, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
    {
        System_printf("tcpHandler: setsockopt failed\n");
        goto shutdown;
    }
    System_printf("tcpHandler: start accepting on file discriptor 0x%08x\n", server);
    System_flush();
    while((clientfd = accept(server, (struct sockaddr *) &clientAddr, &addrlen)) != -1)
    {

        /* Init the Error_Block */
        Error_init(&eb);
        ssl = wolfSSL_new(ctx);
        if(ssl == NULL)
        {
            System_printf("tcpHandler: wolfSSL_new failed.\n");
            close(clientfd);
            goto shutdown;
        }

        wolfSSL_set_fd(ssl, clientfd);

        /* Initialize the defaults and set the parameters. */
        Task_Params_init(&taskParams);
        taskParams.arg0 = (UArg) ssl;
        taskParams.arg1 = (UArg) server;
        taskParams.stackSize = HttpClientSTACKSIZE;
        taskHandle = Task_create((Task_FuncPtr) HttpClient, &taskParams, &eb);
        if(taskHandle == NULL)
        {
            System_printf("tcpHandler: failed to create new Task\n");
            close(clientfd);
        }

        /* addrlen is a value-result param, must reset for next accept call */
        addrlen = sizeof(clientAddr);
    }
    System_printf("tcpHandler: error = 0x%x on file descriptor: 0x%08x\n", errno, server);
    System_printf("tcpHandler: accept failed.\n");
    System_flush();
    shutdown:if(server > 0)
    {
        close(server);
    }
    exitApp(ctx);
}


void HTTPS_vOpen()
{
    Task_Handle taskHandle;
    Task_Params taskParams;
    Error_Block eb;

    /* wolfSSL library needs time() for validating certificates. */
    Seconds_set(CURRENTTIME);

    /* Make sure Error_Block is initialized */
    Error_init(&eb);

    /*
     *  Create the Task that farms out incoming TCP connections.
     *  arg0 will be the port that this task listens to.
     */
    Task_Params_init(&taskParams);
    taskParams.stackSize = HTTPSHANDLERSTACK;
    taskParams.priority = 1;
    taskParams.arg0 = HTTPS_PORT;
    taskHandle = Task_create((Task_FuncPtr)HttpHandler, &taskParams, &eb);
    if (taskHandle == NULL) {
        System_printf("netOpenHook: Failed to create tcpHandler Task\n");
    }
    System_flush();
    AddWebFiles();
}

