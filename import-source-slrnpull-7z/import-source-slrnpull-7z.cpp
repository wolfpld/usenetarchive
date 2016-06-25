#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <direct.h>
#  include <windows.h>
#else
#  include <dirent.h>
#endif

#include "../contrib/lz4/lz4.h"
#include "../contrib/lz4/lz4hc.h"
#include "../contrib/lzma/7z.h"
#include "../contrib/lzma/7zCrc.h"
#include "../contrib/lzma/7zFile.h"
#include "../contrib/lzma/Types.h"
#include "../common/RawImportMeta.hpp"

static bool Exists( const std::string& path )
{
    struct stat buf;
    return stat( path.c_str(), &buf ) == 0;
}

static uint64_t GetFileSize( const char* path )
{
    struct stat buf;
    stat( path, &buf );
    return buf.st_size;
}

static bool CreateDirStruct( const std::string& path )
{
    if( Exists( path ) ) return true;

    if( errno != ENOENT )
    {
        fprintf( stderr, "%s\n", strerror( errno ) );
        return false;
    }

    size_t pos = 0;
    do
    {
        pos = path.find( '/', pos+1 );
#ifdef _WIN32
        if( pos == 2 ) continue;    // Don't create drive name.
        if( _mkdir( path.substr( 0, pos ).c_str() ) != 0 )
#else
        if( mkdir( path.substr( 0, pos ).c_str(), S_IRWXU ) != 0 )
#endif
        {
            if( errno != EEXIST )
            {
                fprintf( stderr, "Creating failed on %s (%s)\n", path.substr( 0, pos ).c_str(), strerror( errno ) );
                return false;
            }
        }
    }
    while( pos != std::string::npos );

    return true;
}

class ExpandingBuffer
{
public:
    ~ExpandingBuffer() { delete[] m_data; }

    char* Request( int size )
    {
        if( size > m_size )
        {
            delete[] m_data;
            m_data = new char[size];
            m_size = size;
        }
        return m_data;
    }

private:
    char* m_data = nullptr;
    int m_size = 0;
};

static void* MemAlloc( void* p, size_t size )
{
    if( size == 0 ) return nullptr;
    return new char[size];
}

static void MemFree( void* p, void* address )
{
    delete[] (char*)address;
}

static ISzAlloc s_alloc = { MemAlloc, MemFree };

static void PosixClose( struct CSzFile* ptr )
{
    if( ptr->ptr )
    {
        fclose( (FILE*)ptr->ptr );
        ptr->ptr = nullptr;
    }
}

static int PosixRead( struct CSzFile* ptr, void* data, size_t* size )
{
    const size_t orig = *size;

    if( orig == 0 ) return 0;

    *size = fread( data, 1, orig, (FILE*)ptr->ptr );
    return ( *size == orig ) ? 0 : ferror( (FILE*)ptr->ptr );
}

static int PosixSeek( struct CSzFile* ptr, Int64* pos, int origin )
{
    int moveMethod;
    int res;
    switch (origin)
    {
    case SZ_SEEK_SET: moveMethod = SEEK_SET; break;
    case SZ_SEEK_CUR: moveMethod = SEEK_CUR; break;
    case SZ_SEEK_END: moveMethod = SEEK_END; break;
    default: return 1;
    }
    res = fseek( (FILE*)ptr->ptr, (long)*pos, moveMethod );
    *pos = ftell( (FILE*)ptr->ptr );
    return res;
}

static int PosixLength( struct CSzFile* ptr, UInt64* length )
{
    const long pos = ftell( (FILE*)ptr->ptr );
    const int res = fseek( (FILE*)ptr->ptr, 0, SEEK_END );
    *length = ftell( (FILE*)ptr->ptr );
    fseek( (FILE*)ptr->ptr, pos, SEEK_SET );
    return res;
}

CFileInStream Mount7z_Posix( FILE* f )
{
    CFileInStream stream;
    stream.file.ptr = f;
    stream.file.close = PosixClose;
    stream.file.read = PosixRead;
    stream.file.seek = PosixSeek;
    stream.file.length = PosixLength;
    return stream;
}

int main( int argc, char** argv )
{
    if( argc != 3 )
    {
        fprintf( stderr, "USAGE: %s input output\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source archive doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[2] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    CFileInStream stream = Mount7z_Posix( fopen( argv[1], "rb" ) );
    CLookToRead lookStream;
    FileInStream_CreateVTable( &stream );
    LookToRead_CreateVTable( &lookStream, False );
    lookStream.realStream = &stream.s;
    LookToRead_Init( &lookStream );

    CSzArEx db;
    CrcGenerateTable();
    SzArEx_Init( &db );
    SzArEx_Open( &db, &lookStream.s, &s_alloc, &s_alloc );

    CreateDirStruct( argv[2] );
    std::string metafn = argv[2];
    metafn.append( "/" );
    std::string datafn = metafn;
    metafn.append( "meta" );
    datafn.append( "data" );
    FILE* meta = fopen( metafn.c_str(), "wb" );
    FILE* data = fopen( datafn.c_str(), "wb" );

    uint64_t offset = 0;

    ExpandingBuffer eb2, fnbuf;
    uint32_t blockIndex = 0;
    char* outBuffer = nullptr;
    size_t outBufferSize = 0;
    size_t lzmaOffset;
    for( int idx=0; idx<db.db.NumFiles; idx++ )
    {
        const CSzFileItem* f = db.db.Files + idx;

        if( ( idx & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", idx, db.db.NumFiles );
            fflush( stdout );
        }

        if( f->IsDir ) continue;

        size_t len = SzArEx_GetFileNameUtf16( &db, idx, nullptr );
        uint16_t* fn = (uint16_t*)fnbuf.Request( 2 * len );
        SzArEx_GetFileNameUtf16( &db, idx, fn );

        if( fn[0] == L'.' ) continue;

        size_t outSizeProcessed;
        SzArEx_Extract( &db, &lookStream.s, idx, &blockIndex,
            (Byte**)&outBuffer, &outBufferSize, &lzmaOffset, &outSizeProcessed,
            &s_alloc, &s_alloc );

        int maxSize = LZ4_compressBound( outSizeProcessed );
        char* compressed = eb2.Request( maxSize );
        int csize = LZ4_compress_HC( outBuffer + lzmaOffset, compressed, outSizeProcessed, maxSize, 16 );

        fwrite( compressed, 1, csize, data );

        RawImportMeta metaPacket = { offset, outSizeProcessed, csize };
        fwrite( &metaPacket, 1, sizeof( RawImportMeta ), meta );
    }
    printf( "%i files processed.\n", db.db.NumFiles );

    fclose( meta );
    fclose( data );

    delete[] outBuffer;
    SzArEx_Free( &db, &s_alloc );

    return 0;
}
