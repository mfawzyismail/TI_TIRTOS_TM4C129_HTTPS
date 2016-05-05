Example Summary
---------------
This application demonstrates how to use TCP with TLS.

Peripherals Exercised
---------------------
Board_LED0      Indicates that the board was initialized within main()
Board_EMAC      Connection to network

Resources & Jumper Settings
---------------------------
Please refer to the development board's specific "Settings and Resources"
section in the Getting Started Guide. For convenience, a short summary is also
shown below.

| Development board | Notes                                                  |
| ================= | ====================================================== |
| DK-TM4C129X       | N/A                                                    |
| EK-TM4C1294XL     |                                                        |
| ----------------- | ------------------------------------------------------ |
| EK-TM4C129EXL     | Supports Crypto Hardware Accelerators                  |
|                   | Use of hardware supported WolfSSL library recommended. |
| ----------------- | ------------------------------------------------------ |

Build Details
-------------
Before building the example:

Change the wallclock time in "tcpEchoTLS.c" marked by "USER STEP" as needed.
WolfSSL uses the time to validate the certificates.

This example requires wolfSSL libraries for TLS. The wolfSSL libraries have to
be separately built and linked with application. Read the following wiki for
detailed instructions for using wolfSSL with TI-RTOS:
    http://processors.wiki.ti.com/index.php?title=Using_WolfSSL_with_TI-RTOS

After building wolfSSL, the below steps are required to link in the necessary
libraries:

    The WolfSSL library <WOLFSSL_lib> is either:
        <WolfSSL_dir>/tirtos/packages/ti/net/wolfssl/lib/wolfssl.a<target>

    Or, for boards with crypto hardware accelerators support:
        <WolfSSL_dir>/tirtos/packages/ti/net/wolfssl/lib/wolfssl_tm4c_hw.a<target>

For TI in CCS: Within the project properties window,
    1. Add the <WolfSSL_dir> path to "Add dir to.." window within
       Build->ARM Compiler->Include Options.

    2. Add the <WOLFSSL_lib> to the Include Library File window within
       Build->ARM Linker-> File Search Path

For IAR: Within the project options window,
    1. Add the <WolfSSL_dir> path to the "Additional include directories"
       window within C/C++ Compiler->Preprocesor

    2. Add the <WolfSSL_lib> path to the "Additional libraries window"
       within Linker->library

For GNU in CCS: Within the project properties window,
    1. Add the <WolfSSL_dir> path to the "Include paths" window within
       Build->GNU Compiler->Directories.

    2. Add ":wolfssl.am4fg" to the "Libraries" window within
       Build->GNU Linker->Libraries

    3. Add the <WolfSSL_dir>/tirtos/packages/ti/net/wolfssl/lib
       to the "library search path" window within Build->GNU Linker->Libraries

For Command line builds:
    The wolfssl library path must be added to the makedefs file to ensure
    the library ordering is correct to avoid linker errors.

    Within the generated TIRTOS examples directory, located within your
    TIRTOS install directory, edit the '<TIRTOS_examples_dir>/GNU/<BOARD_dir>/makedefs'
    file and add the correct WolfSSL library(See above) to the LFLAGS
    variable.

    For reference, example link lines are shown for each toolchain below

For TI Command line builds:

Ex 1)
    LFLAGS = -l"<WOLFSSL_lib>" <LINKERFLAGS> -llibc.a

Ex 2)
    LFLAGS = -l"<WOLFSSL_lib>" -l$(TIVAWARE_INSTALL_DIR)/grlib/ccs/Debug/grlib.lib
    -l$(TIVAWARE_INSTALL_DIR)/driverlib/ccs/Debug/driverlib.lib
    EK_TM4C1294XL.cmd -m$(NAME).map --warn_sections --rom_model
    -i$(CODEGEN_INSTALL_DIR)/lib -llibc.a

For IAR Command line builds:

Ex 1)
  LFLAGS = <WOLFSSL_lib> <LINKERFLAGS>

Ex 2)
    LFLAGS = <WOLFSSL_lib> $(TIVAWARE_INSTALL_DIR)/driverlib/ewarm/Exe/driverlib.a
    --config EK_TM4C1294XL.icf --map $(NAME).map --silent
    --cpu=Cortex-M4F --entry=__iar_program_start

For GNU Command line builds:

    Note that the <WOLFSSL_lib> must appear after the specified linker flags
    but before all math libraries.

Ex 1)
  LFLAGS = <LINKERFLAGS> --gc-sections <WOLFSSL_lib> <MATH_lib>

Ex 2)
    "c:/ti/ccsv6/tools/compiler/gcc-arm-none-eabi-4_8-2014q3/bin/arm-none-eabi-gcc"
    EK_TM4C1294XL.obj httpsget.obj -Wl,-T,EK_TM4C1294XL.lds -Wl,-Map ,httpsget.map
    -Wl,-T,httpsget/linker.cmd -ldriver -march=armv7e-m -mthumb -mfloat-abi=hard
    -mfpu=fpv4-sp-d16 -nostartfiles -static -Wl,--gc-sections <WOLFSSL_lib>
    -lgcc -lc -lm -lrdimon  -o .out

Example Usage
-------------
The device must be connected to a network with a DHCP server to run this example
successfully.

The example turns ON Board_LED0 and starts the WolfSSL stack and the network
stack. When the stack receives an IP address from the DHCP server,
the IP address is written to the console.

To test the example, there are two client tool options we suggest:
Download and run the socat Linux or Windows tool available on internet.

  Usage:  socat stdio openssl-connect:<IP-addr>:<port>,cafile=<WolfSSL-dir>/certs/ca-cert.pem

  <IP-addr>     is the IP address of the application.
  <port>        is the TCP port to being listened to (1000).
  <WolfSSL-dir> is the path to the WolfSSL directory.

  Enter the data to be sent to the tcpEchoTLS server and see the data
  echoed from the tcpEchoTLS server on the screen.

Or, run the WolfSSL echoclient executable to connect to the tcpEchoServer
application running on the target board. The executable can built from the
sources available on WolfSSL manual webpage. Look for Chapter 11 - SSL Tutorial
in the WolfSSL manual for detailed build instructions:
  https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-11-ssl-tutorial.html

  Usage: ./echoclient <IP-addr>

  <IP-addr> is the IP address of the application.

  Make sure the echoclient example is using the correct port (1000).

  Enter the data to be sent to the tcpEchoTLS server and see the data
  echoed from the tcpEchoTLS server on the screen.

Application Design Details
--------------------------
This application uses two types of task:

tcpHandler  Creates a new TLS context, loads certificates, creates a socket and
            accepts incoming connections.  When a connection is established
            a tcpWorker task is dynamically created to send or receive
            data securely.
tcpWorker   Echoes TCP packages back to the client securely.

  'tcpHandler' performs the following actions:
      Create a new TLS context, loads the certificate authority and server
      certificates, and server key to the context buffers.

      Create a socket and bind it to a port (1000 for this example).

      Wait for incoming requests (default: max 3 requests).

      Once a request is received, the request is bound to a TLS instance
      and a new tcpWorker task is dynamically created to manage the
      the communication (echo encrypted TCP packets).

      Waiting for new requests.

  'tcpWorker' performs the following actions:
      Allocate memory to serve as a TCP packet buffer.

      (Calls wolfSSL_read and wolfSSL_write API to decrypt and encrypt
      the communication with the client).

      Receive data from socket client (Data decrypted by TLS layer before it
      is accessible by the user application).

      Echo back TCP packet to the client (Data encrypted by TLS layer before
      sending to the client).

      When client closes the socket, free TLS instance, close server socket,
      free TCP buffer memory and exit the task.

This example can be compared to the tcpEcho example to see the TLS layer that
is added to make the TCP communication secure.

For GNU and IAR users, please read the following website for details about semi-hosting
http://processors.wiki.ti.com/index.php/TI-RTOS_Examples_SemiHosting.
