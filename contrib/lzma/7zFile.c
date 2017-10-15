/* 7zFile.c -- File IO
2009-11-24 : Igor Pavlov : Public domain */

#include "7zFile.h"
#include <errno.h>

WRes File_Close(CSzFile *p)
{
    p->close( p );
    return 0;
}

WRes File_Read(CSzFile *p, void *data, size_t *size)
{
    return p->read( p, data, size );
}

WRes File_Seek(CSzFile *p, Int64 *pos, ESzSeek origin)
{
    return p->seek( p, pos, origin );
}

WRes File_GetLength(CSzFile *p, UInt64 *length)
{
    return p->length( p, length );
}


/* ---------- FileSeqInStream ---------- */

static SRes FileSeqInStream_Read(void *pp, void *buf, size_t *size)
{
  CFileSeqInStream *p = (CFileSeqInStream *)pp;
  return File_Read(&p->file, buf, size) == 0 ? SZ_OK : SZ_ERROR_READ;
}

void FileSeqInStream_CreateVTable(CFileSeqInStream *p)
{
  p->s.Read = FileSeqInStream_Read;
}


/* ---------- FileInStream ---------- */

static SRes FileInStream_Read(void *pp, void *buf, size_t *size)
{
  CFileInStream *p = (CFileInStream *)pp;
  return (File_Read(&p->file, buf, size) == 0) ? SZ_OK : SZ_ERROR_READ;
}

static SRes FileInStream_Seek(void *pp, Int64 *pos, ESzSeek origin)
{
  CFileInStream *p = (CFileInStream *)pp;
  return File_Seek(&p->file, pos, origin);
}

void FileInStream_CreateVTable(CFileInStream *p)
{
  p->s.Read = FileInStream_Read;
  p->s.Seek = FileInStream_Seek;
}
