#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unordered_set>
#include <vector>

#include "../common/Filesystem.hpp"
#include "../common/HashSearch.hpp"
#include "../common/MessageView.hpp"
#include "../common/String.hpp"

extern "C" { time_t parsedate_rfc5322_lax(const char *date); }

struct Message
{
    uint32_t epoch = 0;
    int32_t parent = -1;
    std::vector<uint32_t> children;
};

void Sort( std::vector<uint32_t>& vec, const Message* msg )
{
    std::sort( vec.begin(), vec.end(), [msg]( const uint32_t l, const uint32_t r ) { return msg[l].epoch < msg[r].epoch; } );
}

bool ValidateMsgId( const char* begin, const char* end, char* dst )
{
    bool broken = false;
    while( begin != end )
    {
        if( *begin != ' ' && *begin != '\t' )
        {
            *dst++ = *begin;
        }
        else
        {
            broken = true;
        }
        begin++;
    }
    *dst++ = '\0';
    return broken;
}

int main( int argc, char** argv )
{
    if( argc < 2 )
    {
        fprintf( stderr, "USAGE: %s directory\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Directory doesn't exist.\n" );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    HashSearch hash( base + "middata", base + "midhash", base + "midhashdata" );

    const auto size = mview.Size();

    printf( "Building graph...\n" );
    fflush( stdout );

    unsigned int broken = 0;
    std::unordered_set<std::string> missing;
    std::vector<uint32_t> toplevel;
    auto data = new Message[size];
    char tmp[1024];

    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto buf = post;

        while( strnicmpl( buf, "references: ", 12 ) != 0 && *buf != '\n' )
        {
            buf++;
            while( *buf++ != '\n' ) {}
        }
        if( *buf == '\n' )
        {
            toplevel.push_back( i );
            continue;
        }
        buf += 12;

        const auto terminate = buf;
        int valid = 0;
        while( *buf != '\n' )
        {
            if( *buf == '<' ) valid++;
            else if( *buf == '>' ) valid--;
            buf++;
        }

        if( valid == 0 && buf != terminate )
        {
            buf--;
            for(;;)
            {
                while( *buf != '>' && buf != terminate ) buf--;
                if( buf == terminate )
                {
                    toplevel.push_back( i );
                    break;
                }
                auto end = buf;
                while( *--buf != '<' ) {}
                buf++;
                assert( end - buf < 1024 );
                broken += ValidateMsgId( buf, end, tmp );

                auto idx = hash.Search( tmp );
                if( idx >= 0 )
                {
                    data[i].parent = idx;
                    data[idx].children.emplace_back( i );
                    break;
                }
                else
                {
                    missing.emplace( tmp );
                }

                if( *buf == '>' ) buf--;
            }
        }
    }

    printf( "\nRetrieving timestamps...\n" );
    fflush( stdout );

    unsigned int baddate = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto buf = post;

        while( strnicmpl( buf, "date: ", 6 ) != 0 )
        {
            while( *buf != '\n' ) buf++;
            buf++;
        }
        buf += 6;
        auto end = buf;
        while( *++end != '\n' ) {}
        memcpy( tmp, buf, end-buf );
        tmp[end-buf] = '\0';

        auto date = parsedate_rfc5322_lax( tmp );
        if( date == -1 )
        {
            baddate++;
            date = 0;
        }
        data[i].epoch = date;
    }

    printf( "\nTop level messages: %i\nMissing messages (maybe crosspost): %i\nMalformed references: %i\nUnparsable date fields: %i\n", toplevel.size(), missing.size(), broken, baddate );

    printf( "Sorting top level...\n" );
    fflush( stdout );
    Sort( toplevel, data );
    printf( "Sorting children...\n" );
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }
        if( !data[i].children.empty() )
        {
            Sort( data[i].children, data );
        }
    }

    printf( "\nSaving...\n" );
    FILE* tlout = fopen( ( base + "toplevel" ).c_str(), "wb" );
    fwrite( toplevel.data(), 1, sizeof( uint32_t ) * toplevel.size(), tlout );
    fclose( tlout );

    FILE* cdata = fopen( ( base + "conndata" ).c_str(), "wb" );
    FILE* cmeta = fopen( ( base + "connmeta" ).c_str(), "wb" );
    uint32_t offset = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        fwrite( &offset, 1, sizeof( uint32_t ), cmeta );

        offset += fwrite( &data[i].epoch, 1, sizeof( Message::epoch ), cdata );
        offset += fwrite( &data[i].parent, 1, sizeof( Message::parent ), cdata );
        uint32_t cnum = data[i].children.size();
        offset += fwrite( &cnum, 1, sizeof( cnum ), cdata );
        for( auto& v : data[i].children )
        {
            offset += fwrite( &v, 1, sizeof( v ), cdata );
        }
    }
    fclose( cdata );
    fclose( cmeta );

    printf( "\n" );

    delete[] data;
    return 0;
}
