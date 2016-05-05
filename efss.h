/*
 * efss.h
 *
 *  Created on: May 3, 2016
 *      Author: m.fawzy
 */

#ifndef EFSS_H_
#define EFSS_H_
#include <wolfssl/ssl.h>

/*--------------------------------------------- */
/*--------------------------------------------- */
/* Embedded File System */
/*--------------------------------------------- */
/*--------------------------------------------- */

/* File system equates */
#define EFSS_FILENAME_MAX    80

/* File type equates */
#define EFSS_SEEK_SET        0
#define EFSS_SEEK_CUR        1
#define EFSS_SEEK_END        2
#define EFSS_FILE            void

/* Executable function */
typedef void (*EFSFUN)();
typedef void *         HANDLE;

/* Functions */
extern void      efss_createfile( char *name, unsigned int length, unsigned char *pData );
extern void      efss_createfilecb( char *name, unsigned int length, unsigned char *pData,
                                    EFSFUN pllDestroyFun, unsigned int MemMgrArg );

extern void      efss_destroyfile( char *name );
extern EFSFUN    efss_loadfunction( char *name );

extern EFSS_FILE *efss_fopen( char *name, char *mode );
extern int       efss_fclose( EFSS_FILE *stream );
extern int       efss_feof( EFSS_FILE *stream );
extern size_t    efss_fread( void *ptr, size_t size, size_t nobj,
                            EFSS_FILE *stream );
extern size_t    efss_fwrite( void *ptr, size_t size, size_t nobj,
                             EFSS_FILE *stream );
extern unsigned int     efss_fseek( EFSS_FILE *stream, int offset, int origin );
extern unsigned int     efss_ftell( EFSS_FILE *stream );
extern void      efss_rewind( EFSS_FILE *stream );
extern unsigned int     efss_getfilesize( EFSS_FILE *f );

extern size_t    efss_filesend( EFSS_FILE *stream, size_t size, WOLFSSL *ssl);

extern int  efss_filecheck(char *name, char *user, char *pass, int *prealm);
extern void   *mmAlloc( unsigned int Size );
extern void   mmFree( void* pv );

/* Possible return flags for efss_filecheck() */
#define EFSS_FC_NOTFOUND         0x01        /* File not found */
#define EFSS_FC_NOTALLOWED       0x02        /* File can not be accessed */
#define EFSS_FC_EXECUTE          0x04        /* Filename is a function call (CGI) */
#define EFSS_FC_AUTHFAILED       0x08        /* File autentication failed */
                                            /* (realm index suppied in realm) */




#endif /* EFSS_H_ */
