#include <algorithm>
#include <assert.h>
#include <chrono>
#include <inttypes.h>
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
#include "../common/FileMap.hpp"
#include "../common/MessageLogic.hpp"
#include "../common/MessageView.hpp"
#include "../common/MsgIdHash.hpp"
#include "../common/String.hpp"

struct HashData
{
    uint32_t offset;
    uint32_t idx;
};

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

    std::vector<const char*> msgidvec;
    msgidvec.reserve( size );

    uint32_t offset = 0;
    ExpandingBuffer eb;
    uint32_t zero = 0;
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
        auto tmp = new char[slen+1];
        memcpy( tmp, buf, slen );
        tmp[slen] = '\0';
        msgidvec.emplace_back( tmp );
    }

    auto hashbits = MsgIdHashBits( size, 75 );
    auto hashsize = MsgIdHashSize( hashbits );
    auto hashmask = MsgIdHashMask( hashbits );
    auto bucket = new std::vector<HashData>[hashsize];

    std::string midmetafn = base + "midmeta";
    std::string middatafn = base + "middata";

    FILE* midmeta = fopen( midmetafn.c_str(), "wb" );
    FILE* middata = fopen( middatafn.c_str(), "wb" );

    for( uint32_t i=0; i<size; i++ )
    {
        const auto msg = msgidvec[i];
        const auto len = strlen( msg );
        fwrite( msg, 1, len+1, middata );

        fwrite( &offset, 1, sizeof( offset ), midmeta );

        uint32_t hash = XXH32( msg, len, 0 ) & hashmask;
        bucket[hash].emplace_back( HashData { offset, i } );

        offset += len+1;
    }

    fclose( midmeta );
    fclose( middata );

    printf( "Processed %i MsgIDs.\n", size );

    std::string midhashfn = base + "midhash";
    std::string midhashdatafn = base + "midhashdata";

    FILE* midhash = fopen( midhashfn.c_str(), "wb" );
    FILE* midhashdata = fopen( midhashdatafn.c_str(), "wb" );

    FileMap<char> msgid( middatafn );
    fwrite( &zero, 1, sizeof( uint32_t ), midhashdata );
    offset = sizeof( uint32_t );
    for( uint32_t i=0; i<hashsize; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%i\r", i, hashsize );
            fflush( stdout );
        }

        std::sort( bucket[i].begin(), bucket[i].end(), [&msgid]( const HashData& l, const HashData& r ) { return strcmp( msgid + l.offset, msgid + r.offset ) > 0; } );

        uint32_t num = bucket[i].size();
        if( num == 0 )
        {
            fwrite( &zero, 1, sizeof( uint32_t ), midhash );
        }
        else
        {
            fwrite( &offset, 1, sizeof( offset ), midhash );
            fwrite( &num, 1, sizeof( num ), midhashdata );
            fwrite( bucket[i].data(), 1, num * sizeof( HashData ), midhashdata );
            offset += sizeof( num ) + num * sizeof( HashData );
        }
    }

    fclose( midhash );
    fclose( midhashdata );

    delete[] bucket;

    printf( "Processed %i buckets.\n", hashsize );

    return 0;
}
