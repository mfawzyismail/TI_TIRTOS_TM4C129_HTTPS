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
 * Embedded file system
 *
 * This file contains a file IO API similar to standard C, which allows
 * it to be easily ported to a system which has its own file system.
 *
 */
#include <string.h>
#include <xdc/std.h>
//#include <netmain.h>
//#include <_oskern.h>
#include <wolfssl/ssl.h>
#include "efss.h"
//#include <ti/ndk/inc/nettools/netcfg.h>

/* Kernel HTYPE's */
#define HTYPE_CFG               0x0102
#define HTYPE_CFGENTRY          0x0103
#define HTYPE_EFSFILEHEADER     0x0104
#define HTYPE_EFSFILEPOINTER    0x0105
/* Authority consists of flags. One or more of the following can be set... */
#define CFG_ACCTFLG_CH1         0x1000
#define CFG_ACCTFLG_CH2         0x2000
#define CFG_ACCTFLG_CH3         0x4000
#define CFG_ACCTFLG_CH4         0x8000
#define CFG_ACCTFLG_CHALL       0xF000

/* Account Item Types */
#define CFGITEM_ACCT_SYSTEM     1
#define CFGITEM_ACCT_PPP        1
#define CFGITEM_ACCT_REALM      1


/*--------------------------------------------------------------------------- */
/* Defined Configuration Tags */
/*--------------------------------------------------------------------------- */
#define CFGTAG_OS               0x0001          /* OS Configuration */
#define CFGTAG_IP               0x0002          /* IP Stack Configuration */
#define CFGTAG_SERVICE          0x0003          /* Service */
#define CFGTAG_IPNET            0x0004          /* IP Network */
#define CFGTAG_ROUTE            0x0005          /* Gateway Route */
#define CFGTAG_CLIENT           0x0006          /* DHCPS Client */
#define CFGTAG_SYSINFO          0x0007          /* System Information */
#define CFGTAG_ACCT             0x0008          /* User Account */

/* Internal File Header */
typedef struct _fileheader
     {
        unsigned int               Type;
        struct _fileheader *pNext;
        char               Filename[EFSS_FILENAME_MAX];
        int              Length;
        int              RefCount;
        unsigned int               Flags;
#define EFSFLG_DESTROY_PENDING  0x0001
        EFSFUN             pMemMgrCallback;
        unsigned int             MemMgrArg;
        unsigned char              *pData;
     } FILEHEADER;

/* Internal File Pointer */
typedef struct _fileptr
     {
        unsigned int               Type;
        struct _fileheader *pfh;
        int              Pos;
     } FILEPTR;

static FILEHEADER *pRoot = 0;

/* Kernel Level Gateway Functions */
extern void  llEnter();
extern void  llExit();
extern int  CfgEntryDeRef( HANDLE hCfgEntry );
extern int  CfgEntryGetData( HANDLE hCfgEntry, int *pSize, unsigned char *pData );
extern int  CfgGetEntry( HANDLE hCfg, unsigned int Tag, unsigned int Item,
                         unsigned int Index, HANDLE *phCfgEntry );
extern int  CfgGetNextEntry( HANDLE hCfg, HANDLE hCfgEntry,
                             HANDLE *phCfgEntryNext );
extern int stricmp(const char *s1, const char *s2);
/*-------------------------------------------------------------------- */
/*  Local Functions */
/*-------------------------------------------------------------------- */

static FILEHEADER *efss_findfile( char *name );
static void        efss_internal_remove( FILEHEADER *pfh );


/*-------------------------------------------------------------------- */
/* efss_findfile */

/* Finds file by name, and potentially removes file */

/* CALLED IN KERNEL MODE */
/*-------------------------------------------------------------------- */
FILEHEADER *efss_findfile( char *name )
{
    FILEHEADER *pfh;

    /* Find */
    pfh     = pRoot;

    while( pfh )
    {
        if( !(pfh->Flags & EFSFLG_DESTROY_PENDING) &&
                      !stricmp( name, pfh->Filename ) )
            break;
        pfh = pfh->pNext;
    }

    return( pfh );
}

/*-------------------------------------------------------------------- */
/* efss_internal_remove */

/* Removes file */

/* CALLED IN KERNEL MODE */
/*-------------------------------------------------------------------- */
static void efss_internal_remove( FILEHEADER *pfh )
{
    FILEHEADER *pfhTmp;
    FILEHEADER *pfhPrev;

    pfhTmp  = pRoot;
    pfhPrev = 0;

    while( pfhTmp )
    {
        if( pfhTmp == pfh )
            break;
        else
        {
            pfhPrev = pfhTmp;
            pfhTmp = pfhTmp->pNext;
        }
    }

    if( !pfhTmp )
        return;

    /* Remove */
    /* pfh is entry to remove */
    if( !pfhPrev )
        pRoot = pfh->pNext;
    else
        pfhPrev->pNext = pfh->pNext;

    /* Zap type for debug */
    pfh->Type = 0;
    if( pfh->pMemMgrCallback )
        (pfh->pMemMgrCallback)( pfh->MemMgrArg );

    mmFree( pfh );
}


/*-------------------------------------------------------------------- */
/* efss_createfile */
/* Creates a new file from a given name, length and data pointer */
/*-------------------------------------------------------------------- */
void efss_createfile( char *name, unsigned int length, unsigned char *pData )
{
    efss_createfilecb( name, length, pData, 0, 0 );
}


/*-------------------------------------------------------------------- */
/* efss_createfilecb */
/* Creates a new file from a given name, length and data pointer */
/*-------------------------------------------------------------------- */
void efss_createfilecb( char *name, unsigned int length, unsigned char *pData,
                       EFSFUN pllDestroyFun, unsigned int MemMgrArg )
{
    FILEHEADER *pfh;

    /* Allocate the file header structure */
    pfh = mmAlloc( sizeof(FILEHEADER) );
    if( !pfh )
        return;

    pfh->Type = HTYPE_EFSFILEHEADER;

    /* Record the filename, size, and data pointer */
    strncpy( pfh->Filename, name, EFSS_FILENAME_MAX-1 );
    pfh->Filename[EFSS_FILENAME_MAX-1] = 0;
    pfh->Length     = length;
    pfh->RefCount   = 0;
    pfh->pData      = pData;
    pfh->Flags      = 0;
    pfh->pMemMgrCallback = pllDestroyFun;
    pfh->MemMgrArg = MemMgrArg;

    llEnter();

    /* Put it in our list */
    pfh->pNext = pRoot;
    pRoot      = pfh;

    llExit();

}


/*-------------------------------------------------------------------- */
/* efss_destroyfile */
/* Removes a file from the list and frees header */
/*-------------------------------------------------------------------- */
void efss_destroyfile( char *name )
{
    FILEHEADER *pfh;

    llEnter();

    pfh = efss_findfile( name );
    if( !pfh )
        goto dest_exit;

    if( !pfh->RefCount )
        efss_internal_remove( pfh );
    else
        pfh->Flags |= EFSFLG_DESTROY_PENDING;

dest_exit:
    llExit();

}

/*-------------------------------------------------------------------- */
/* efss_loadfunction */
/* Loads a function into memory by filename and returns a function */
/* pointer (or NULL on error) */
/*-------------------------------------------------------------------- */
EFSFUN efss_loadfunction( char *name )
{
    FILEHEADER *pfh;
    EFSFUN     pFun = 0;

    llEnter();

    pfh = efss_findfile( name );
    if( pfh ) {
        /* CQ7924 fix. To eliminate compiler warning. */
        unsigned int temp;

        temp = (unsigned int) pfh->pData;
        pFun = (EFSFUN) temp;
    }

    llExit();

    return( pFun );
}

/*-------------------------------------------------------------------- */
/* efss_fopen */
/* Opens a file and returns file pointer (or NULL on error) */
/*-------------------------------------------------------------------- */
EFSS_FILE *efss_fopen( char *name, char *mode )
{
    FILEHEADER *pfh;
    FILEPTR    *pf;

    /* Verify the read mode */
    if( stricmp( mode, "rb" ) )
        return(0);

    llEnter();

    pfh = efss_findfile( name );

    if( !pfh )
    {
        llExit();
        return(0);
    }

    /* Allocate the file header structure */
    pf = mmAlloc( sizeof(FILEPTR) );
    if( !pf )
    {
        llExit();
        return(0);
    }

    pf->Type = HTYPE_EFSFILEPOINTER;
    pf->pfh = pfh;
    pf->Pos = 0;

    pfh->RefCount++;

    llExit();

    return( pf );
}

/*-------------------------------------------------------------------- */
/* efss_fclose */
/* Closes a file */
/*-------------------------------------------------------------------- */
int efss_fclose( EFSS_FILE *stream )
{
    FILEPTR *pf     = (FILEPTR *)stream;
    FILEHEADER *pfh = pf->pfh;

    llEnter();

    /* Zap type for debug */
    pf->Type = 0;
    mmFree( pf );

    pfh->RefCount--;

    if( !pfh->RefCount && (pfh->Flags & EFSFLG_DESTROY_PENDING) )
        efss_internal_remove( pfh );

    llExit();

    return(0);
}

/*-------------------------------------------------------------------- */
/* efss_feof */
/* Returns non-zero if at end of file */
/*-------------------------------------------------------------------- */
int efss_feof( EFSS_FILE *stream )
{
    FILEPTR    *pf  = (FILEPTR *)stream;
    FILEHEADER *pfh = pf->pfh;

    if( pf->Pos >= pfh->Length )
        return(1);
    return(0);
}

/*-------------------------------------------------------------------- */
/* efss_fread */
/* Returns number of objects read (set size to 1 to read bytes) */
/*-------------------------------------------------------------------- */
size_t efss_fread( void *ptr, size_t size, size_t nobj, EFSS_FILE *stream )
{
    FILEPTR    *pf  = (FILEPTR *)stream;
    FILEHEADER *pfh = pf->pfh;
    int       canread;

    /* Check for null operation */
    if( !size || !nobj )
        return(0);

    /* Get number of bytes we can read */
    canread = pfh->Length - pf->Pos;

    /* Get number of objects we can read */
    canread /= (int)size;

    /* Check for max user request */
    if( canread > (int)nobj )
        canread = (int)nobj;

    /* Copy what we can */
    if( canread )
        memcpy( ptr, pfh->pData + pf->Pos, (size_t)canread*size );

    /* Bump file position */
    pf->Pos += canread*(int)size;

    /* return what we read */
    return( (size_t)canread * size );
}

/*-------------------------------------------------------------------- */
/* efss_fwrite */
/* Returns zero - we don't write */
/*-------------------------------------------------------------------- */
size_t efss_fwrite( void *ptr, size_t size, size_t nobj, EFSS_FILE *stream )
{
    (void)ptr;
    (void)size;
    (void)nobj;
    (void)stream;

    return(0);
}

/*-------------------------------------------------------------------- */
/* efss_fseek */
/* Returns non-zero on error */
/*-------------------------------------------------------------------- */
unsigned int efss_fseek( EFSS_FILE *stream, int offset, int origin )
{
    FILEPTR    *pf  = (FILEPTR *)stream;
    FILEHEADER *pfh = pf->pfh;

    switch( origin )
    {
    case EFSS_SEEK_SET:
        goto seek_bound;

    case EFSS_SEEK_CUR:
        offset += pf->Pos;
        goto seek_bound;

    case EFSS_SEEK_END:
        offset += pfh->Length;
seek_bound:
        if( offset < 0 )
            offset = 0;
        if( offset > pfh->Length )
            offset = pfh->Length;
        pf->Pos = offset;
        break;

    default:
        break;
    }

    return(0);
}

/*-------------------------------------------------------------------- */
/* efss_ftell */
/* Returns the current file position */
/*-------------------------------------------------------------------- */
unsigned int efss_ftell( EFSS_FILE *stream )
{
    FILEPTR *pf  = (FILEPTR *)stream;
    return( pf->Pos );
}

/*-------------------------------------------------------------------- */
/* efss_rewind */
/* Rewind the file */
/*-------------------------------------------------------------------- */
void efss_rewind( EFSS_FILE *stream )
{
    FILEPTR *pf  = (FILEPTR *)stream;
    pf->Pos = 0;
}

/*-------------------------------------------------------------------- */
/* efss_getfilesize */
/* Inputs: */
/* stream :  File pointer */
/* Returns:   filesize  : file size */
/*-------------------------------------------------------------------- */
unsigned int efss_getfilesize( EFSS_FILE *stream )
{
    FILEPTR *pf  = (FILEPTR *)stream;
    return( pf->pfh->Length );
}


/*-------------------------------------------------------------------- */
/* efss_filesend */

/* Inputs: */
/* stream :  File pointer */
/* s      :  Socket */

/* Sends data for the file represented by "stream" directly to */
/* socket "s", in the most efficient manner possible. */

/* Returns:  The number of bytes sent */
/*-------------------------------------------------------------------- */
size_t efss_filesend( EFSS_FILE *stream, size_t size, WOLFSSL *ssl )
{
    FILEPTR *pf     = (FILEPTR *)stream;
    FILEHEADER *pfh = pf->pfh;
    int           temp;

    if( !size )
        return(0);

    /* Get number of bytes we can read */
    temp = pfh->Length - pf->Pos;

    /* Get the number of bytes to copy */
    if( temp < size )
        size = temp;

    /* Send it right to the sockets layer */
    temp = wolfSSL_send( ssl, pfh->pData + pf->Pos, size, 0);
    if( temp<0 )
        temp = 0;

    pf->Pos += temp;

    return( temp );
}


/*------------------------------------------------------------------------- */
/* efss_filecheck */

/* Inputs: */
/* name :  Null terminated filename string */
/* user :  Null terminated userid string */
/* pass:   Null terminated password string */
/* prealm: Pointer to store realm index when EFSS_FC_AUTHFAILED returned */

/* Returns:  An integer consisting of one of more of the following flags */
/*  EFSS_FC_NOTFOUND    - File not found */
/*  EFSS_FC_NOTALLOWED  - File can not be accessed */
/*  EFSS_FC_EXECUTE     - Filename is a function call (CGI) */
/*  EFSS_FC_AUTHFAILED  - File autentication failed (failing realm supplied) */

/*------------------------------------------------------------------------- */
int efss_filecheck(char *name, char *user, char *pass, int *prealm)
{
    FILEHEADER *pfh;
    char tstr[EFSS_FILENAME_MAX];
    int  i,realm=0,retflags = 0;

    /* First, see if the file is present */

    llEnter();
    pfh = efss_findfile( name );
    llExit();

    if( !pfh )
        return( EFSS_FC_NOTFOUND );

    /* Next, determine if the file is a "function". */

    /* The method for detecting if a filename represents an executable */
    /* function is determined by this EFS module. It can use any method */
    /* suitable to application developer. */

    /* For this implementation, we assume if the file ends in '.cgi', */
    /* then its a function. */

    i = strlen(name);
    if( i>4 && !stricmp( name+i-4, ".cgi" ) )
        retflags |= EFSS_FC_EXECUTE;


    /* Now do authentication */

    /* The authentication method is determined by this EFS module. */
    /* It can use any method suitable to application developer. */

    /* For this implementation, the authentication scheme works by */
    /* looking for files named %R% in primary subdirectory of the */
    /* requested file, or in the root directory if no subdirectory */
    /* exits. The file contains a single 4 byte int that is the */
    /* authorization realm index. If there is no file, there is no */
    /* authentication. Here, only the first subdir level is */
    /* validated. */

    /* Find the first '/' terminator (if any). */
    /* We must have room for "$R$<NULL>" at the end */
    for( i=0; i<(EFSS_FILENAME_MAX-4); i++ )
    {
        tstr[i] = *(name+i);
        if( tstr[i] == '/' || !tstr[i] )
            break;
    }

    /* Setup the following: */
    /*   tstr   = the primary directory name "mydir/" (null terminated) */
    /*   name+i = the part of the filename following the primary directory */
    /*   Return EFSS_FC_NOTALLOWED on an error */
    if( tstr[i] == '/' )
        tstr[++i]=0;
    else if( !tstr[i] )
    {
        tstr[0] = 0;
        i=0;
    }
    else
    {
        retflags |= EFSS_FC_NOTALLOWED;
        return(retflags);
    }

    /* First of all, it is illegal to request a %R% file */
    if( !stricmp( name+i, "%R%" ) )
    {
        retflags |= EFSS_FC_NOTALLOWED;
        return(retflags);
    }

    /* Look for a realm authentication file */
    /* and read the required realm index */
    strcat( tstr, "%R%" );

    /* (Since we're EFS, we'll cheat and use the internal EFS functions) */
    llEnter();
    pfh = efss_findfile( tstr );
    if( pfh )
        realm = *(int *)pfh->pData;
    llExit();

    /* If there is no realm (or the realm is zero), we're done */
    if( !realm )
        return(retflags);

    /* If we have either a username or password, attempt the validation. */
    if( *user || *pass )
    {
#if 0
        HANDLE  hAcct;
        CI_ACCT CA;
        int     rc;
        int     size;

        /* i == "Found" flag */
        i = 0;

        /* Search for this user account */
        rc = CfgGetEntry( 0, CFGTAG_ACCT, CFGITEM_ACCT_REALM, 1, &hAcct );
        while( rc > 0 )
        {
            /* Read the account data in our CA record */
            size = sizeof(CA);
            rc = CfgEntryGetData( hAcct, &size, (unsigned char *)&CA );

            /* If the read succeeded and the username matches, we're done */
            if( rc > 0 && !strcmp( user, CA.Username ) )
            {
                i = 1;
                CfgEntryDeRef( hAcct );
                break;
            }

            /* Close the current handle on error, or get next handle */
            if( rc <= 0 )
                CfgEntryDeRef( hAcct );
            else
                rc = CfgGetNextEntry( 0, hAcct, &hAcct );
        }

        /* If we found the user, validate the password */
        if( i && !strcmp( pass, CA.Password ) )
        {
            /* See if the user is authorized for this realm */
            if( CA.Flags & (CFG_ACCTFLG_CH1<<(realm-1)) )
                return(retflags);
        }
#endif
    }

    /* Return the non-validated realm index */
    retflags |= EFSS_FC_AUTHFAILED;
    if( prealm )
        *prealm = realm;
    return(retflags);
}

