/* 7zFile.h -- File IO
2009-11-24 : Igor Pavlov : Public domain */

#ifndef __7Z_FILE_H
#define __7Z_FILE_H

#include <stdio.h>

#include "Types.h"

EXTERN_C_BEGIN

/* ---------- File ---------- */

struct CSzFile;
typedef struct CSzFile
{
    void* ptr;
    void (*close)( struct CSzFile* ptr );
    int (*read)( struct CSzFile* ptr, void* data, size_t* size );
    int (*seek)( struct CSzFile* ptr, Int64* pos, int origin );
    int (*length)( struct CSzFile* ptr, UInt64* length );
} CSzFile;

WRes File_Close(CSzFile *p);

/* reads max(*size, remain file's size) bytes */
WRes File_Read(CSzFile *p, void *data, size_t *size);

WRes File_Seek(CSzFile *p, Int64 *pos, ESzSeek origin);
WRes File_GetLength(CSzFile *p, UInt64 *length);


/* ---------- FileInStream ---------- */

typedef struct
{
  ISeqInStream s;
  CSzFile file;
} CFileSeqInStream;

void FileSeqInStream_CreateVTable(CFileSeqInStream *p);


typedef struct
{
  ISeekInStream s;
  CSzFile file;
} CFileInStream;

void FileInStream_CreateVTable(CFileInStream *p);

EXTERN_C_END

#endif
