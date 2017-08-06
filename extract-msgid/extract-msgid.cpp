#include <algorithm>
#include <assert.h>
#include <chrono>
#include <inttypes.h>
#include <limits>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <random>
#include <vector>

#include "../contrib/xxhash/xxhash.h"
#include "../common/Filesystem.hpp"
#include "../common/MessageLogic.hpp"
#include "../common/MessageView.hpp"
#include "../common/MsgIdHash.hpp"
#include "../common/Slab.hpp"
#include "../common/String.hpp"
#include "../common/StringCompress.hpp"

void CreateDummyMsgId( const char*& begin, const char*& end, int idx )
{
    static char buf[1024];
    static std::random_device rd;

    int64_t t = std::chrono::duration_cast<std::chrono::nanoseconds>( std::chrono::high_resolution_clock::now().time_since_epoch() ).count();
    const auto r = uint32_t{ rd() };

    sprintf( buf, "uat.%i.%" PRId64 ".%i@usenet.archive.toolkit", idx, t, r );

    begin = buf;
    end = begin;
    while( *end != '\0' ) end++;
}

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s raw\n", argv[0] );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const auto size = mview.Size();

    std::vector<const char*> rawmsgidvec;
    rawmsgidvec.reserve( size );

    Slab<32*1024*1024> slab;
    ExpandingBuffer eb;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto buf = FindOptionalHeader( post, "message-id: ", 12 );
        if( *buf == '\n' )
        {
            buf = FindOptionalHeader( post, "message-id:\t", 12 );
        }
        const char* end;
        if( *buf != '\n' )
        {
            buf += 12;
            end = buf;
            while( *buf != '<' && *buf != '\n' ) buf++;
            if( *buf == '\n' )
            {
                std::swap( end, buf );
                if( !IsMsgId( buf, end ) )
                {
                    fprintf( stderr, "Broken Message-Id: in message %i!\n", i );
                    CreateDummyMsgId( buf, end, i );
                }
            }
            else
            {
                buf++;
                end = buf;
                while( *end != '>' && *end != '\n' ) end++;

                if( !IsMsgId( buf, end ) )
                {
                    fprintf( stderr, "Broken Message-Id: in message %i!\n", i );
                    CreateDummyMsgId( buf, end, i );
                }
            }
        }
        else
        {
            fprintf( stderr, "No Message-Id: header in message %i!\n", i );
            CreateDummyMsgId( buf, end, i );
        }

        const auto slen = end-buf;
        auto tmp = (char*)slab.Alloc( slen+1 );
        memcpy( tmp, buf, slen );
        tmp[slen] = '\0';
        rawmsgidvec.emplace_back( tmp );
    }

    printf( "Processed %i MsgIDs.\n", size );

    printf( "Building code book...\n" );
    fflush( stdout );
    const StringCompress compress( rawmsgidvec );
    compress.WriteData( base + "msgid.codebook" );

    std::vector<const uint8_t*> msgidvec;
    msgidvec.reserve( size );
    for( int i=0; i<size; i++ )
    {
        if( ( i & 0x3FFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        uint8_t tmp[2048];
        const auto sz = compress.Pack( rawmsgidvec[i], tmp );
        auto ptr = (uint8_t*)slab.Alloc( sz );
        memcpy( ptr, tmp, sz );
        msgidvec.emplace_back( ptr );
    }
    printf( "\n" );

    auto hashbits = MsgIdHashBits( size, 90 );
    auto hashsize = MsgIdHashSize( hashbits );
    auto hashmask = MsgIdHashMask( hashbits );

    auto hashdata = new uint32_t[hashsize];
    auto distance = new uint8_t[hashsize];
    memset( distance, 0xFF, hashsize );
    uint8_t distmax = 0;

    for( uint32_t i=0; i<size; i++ )
    {
        const auto msg = msgidvec[i];
        const auto len = strlen( (const char*)msg );

        uint32_t hash = XXH32( msg, len, 0 ) & hashmask;
        uint8_t dist = 0;
        uint32_t idx = i;
        for(;;)
        {
            if( distance[hash] == 0xFF )
            {
                if( distmax < dist ) distmax = dist;
                distance[hash] = dist;
                hashdata[hash] = idx;
                break;
            }
            if( distance[hash] < dist )
            {
                if( distmax < dist ) distmax = dist;
                std::swap( distance[hash], dist );
                std::swap( hashdata[hash], idx );
            }
            dist++;
            assert( dist < std::numeric_limits<uint8_t>::max() );
            hash = (hash+1) & hashmask;
        }
    }

    FILE* meta = fopen( ( base + "midhashdata" ).c_str(), "wb" );
    fwrite( &distmax, 1, 1, meta );
    fclose( meta );

    FILE* data = fopen( ( base + "midhash" ).c_str(), "wb" );
    FILE* strdata = fopen( ( base + "middata" ).c_str(), "wb" );
    FILE* strmeta = fopen( ( base + "midmeta" ).c_str(), "wb" );

    const uint32_t zero = 0;
    uint32_t stroffset = fwrite( &zero, 1, 1, strdata );

    auto msgidoffset = new uint32_t[size];

    int cnt = 0;
    for( int i=0; i<hashsize; i++ )
    {
        if( ( i & 0x3FFF ) == 0 )
        {
            printf( "%i/%i\r", i, hashsize );
            fflush( stdout );
        }

        if( distance[i] == 0xFF )
        {
            fwrite( &zero, 1, sizeof( uint32_t ), data );
            fwrite( &zero, 1, sizeof( uint32_t ), data );
        }
        else
        {
            fwrite( &stroffset, 1, sizeof( uint32_t ), data );
            fwrite( hashdata+i, 1, sizeof( uint32_t ), data );

            msgidoffset[hashdata[i]] = stroffset;
            cnt++;
            auto str = msgidvec[hashdata[i]];
            stroffset += fwrite( str, 1, strlen( (const char*)str ) + 1, strdata );
        }
    }

    assert( cnt == size );
    fwrite( msgidoffset, 1, size * sizeof( uint32_t ), strmeta );

    fclose( data );
    fclose( strdata );
    fclose( strmeta );

    printf( "%i/%i\n", hashsize, hashsize );

    return 0;
}
