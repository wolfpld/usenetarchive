#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>

#include "../contrib/lz4/lz4.h"
#include "../contrib/xxhash/xxhash.h"
#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/MsgIdHash.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"

#define CSTR(x) strcmp( argv[2], x ) == 0

enum class Mode
{
    Invalid,
    Info,
    List,
    QueryMsgId,
    QueryPost
};

static void PrintPost( uint32_t idx, const RawImportMeta* meta, const char* data )
{
    ExpandingBuffer eb;
    auto postsize = meta[idx].size;
    auto post = eb.Request( postsize );
    auto dec = LZ4_decompress_fast( data + meta[idx].offset, post, postsize );
    assert( dec == meta[idx].compressedSize );

    for( int i=0; i<postsize; i++ )
    {
        putchar( *post++ );
    }
    printf( "\n" );
}

int main( int argc, char** argv )
{
    if( argc < 3 )
    {
        fprintf( stderr, "USAGE: %s data params\n\nParams:\n", argv[0] );
        fprintf( stderr, "  -i            general information\n" );
        fprintf( stderr, "  -l            list all Message IDs\n" );
        fprintf( stderr, "  -m msgid      query Message ID\n" );
        fprintf( stderr, "  -q id         query post\n" );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }

    Mode mode = Mode::Invalid;

    if( CSTR( "-i" ) )
    {
        mode = Mode::Info;
    }
    else if( CSTR( "-l" ) )
    {
        mode = Mode::List;
    }
    else if( CSTR( "-m" ) )
    {
        if( argc < 4 )
        {
            fprintf( stderr, "Missing Message ID.\n" );
            exit( 1 );
        }
        mode = Mode::QueryMsgId;
    }
    else if( CSTR( "-q" ) )
    {
        if( argc < 4 )
        {
            fprintf( stderr, "Missing post number.\n" );
            exit( 1 );
        }
        mode = Mode::QueryPost;
    }

    if( mode == Mode::Invalid )
    {
        fprintf( stderr, "Invalid mode of operation.\n" );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );

    FileMap<RawImportMeta> meta( base + "meta" );
    FileMap<char> data( base + "data" );
    FileMap<char> middata( base + "middata" );
    FileMap<uint32_t> midmeta( base + "midmeta" );
    FileMap<uint32_t> midhash( base + "midhash" );
    FileMap<uint32_t> midhashdata( base + "midhashdata" );

    auto size = meta.Size() / sizeof( RawImportMeta );

    if( mode == Mode::Info )
    {
        printf( "Raw archive %s\n", argv[1] );
        printf( "  Number of posts: %i\n", size );
    }
    else if( mode == Mode::List )
    {
        for( uint32_t i=0; i<size; i++ )
        {
            printf( "%s\n", middata + midmeta[i] );
        }
    }
    else if( mode == Mode::QueryMsgId )
    {
        auto hash = XXH32( argv[3], strlen( argv[3] ), 0 ) & MsgIdHashMask;
        auto offset = midhash[hash] / sizeof( uint32_t );
        const uint32_t* ptr = midhashdata + offset;
        auto num = *ptr++;
        for( int j=0; j<num; j++ )
        {
            if( strcmp( argv[3], middata + *ptr++ ) == 0 )
            {
                PrintPost( *ptr, meta, data );
                return 0;
            }
            ptr++;
        }
        fprintf( stderr, "Invalid Message ID %s\n", argv[3] );
        exit( 1 );
    }
    else if( mode == Mode::QueryPost )
    {
        auto idx = atoi( argv[3] );
        if( idx < 0 || idx >= size )
        {
            fprintf( stderr, "Index out of range: 0 <= %i < %i\n", idx, size );
            exit( 1 );
        }
        PrintPost( idx, meta, data );
    }

    return 0;
}
