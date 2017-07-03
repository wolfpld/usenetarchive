#include <algorithm>
#include <assert.h>
#include <functional>
#include <limits>
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unordered_set>
#include <vector>

#include "../common/Filesystem.hpp"
#include "../common/HashSearch.hpp"
#include "../common/MessageLogic.hpp"
#include "../common/MessageView.hpp"
#include "../common/String.hpp"

extern "C" { time_t parsedate_rfc5322_lax(const char *date); }

struct Message
{
    uint32_t epoch = 0;
    int32_t parent = -1;
    uint32_t childTotal = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> children;
};

void Sort( std::vector<uint32_t>& vec, const Message* msg )
{
    std::sort( vec.begin(), vec.end(), [msg]( const uint32_t l, const uint32_t r ) { return msg[l].epoch < msg[r].epoch; } );
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

        auto buf = FindReferences( post );
        if( *buf == '\n' )
        {
            toplevel.push_back( i );
            continue;
        }

        const auto terminate = buf;
        int valid = ValidateReferences( buf );
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
        else
        {
            toplevel.push_back( i );
        }
    }

    printf( "\nRetrieving timestamps...\n" );
    fflush( stdout );

    std::vector<const char*> received;
    unsigned int baddate = 0;
    unsigned int recdate = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto buf = post;
        received.clear();

        while( *buf != '\n' )
        {
            if( strnicmpl( buf, "received: ", 10 ) == 0 )
            {
                buf += 10;
                while( *buf != ';' && *buf != '\n' ) buf++;
                if( *buf == ';' )
                {
                    buf++;
                    while( *buf == ' ' || *buf == '\t' ) buf++;
                    received.emplace_back( buf );
                }
            }
            while( *buf++ != '\n' ) {}
        }

        time_t recvdate = -1;
        for( int i=received.size()-1; i>=0; i-- )
        {
            recvdate = parsedate_rfc5322_lax( received[i] );
            if( recvdate != -1 ) break;
        }

        buf = post;
        while( strnicmpl( buf, "date: ", 6 ) != 0 && strnicmpl( buf, "date:\t", 6 ) != 0 )
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
            if( recvdate == -1 )
            {
                struct tm tm = {};
                if( sscanf( buf, "%d/%d/%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday ) == 3 )
                {
                    if( tm.tm_year >= 1970 && tm.tm_mon <= 12 && tm.tm_mday <= 31 )
                    {
                        tm.tm_year -= 1900;
                        tm.tm_mon--;
                        date = mktime( &tm );
                    }
                    else
                    {
                        date = 0;
                    }
                }
                else
                {
                    date = 0;
                }
            }
            else
            {
                recdate++;
                date = recvdate;
            }
        }
        data[i].epoch = date;
    }

    unsigned int loopcnt = 0;
    std::unordered_set<uint32_t> visited;
    printf( "\nChild count...\n" );
    fflush( stdout );
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        visited.clear();
        auto idx = i;
        for(;;)
        {
            auto parent = data[idx].parent;
            if( parent == -1 ) break;
            if( visited.find( parent ) != visited.end() )
            {
                loopcnt++;
                data[idx].parent = -1;
                auto it = std::find( data[parent].children.begin(), data[parent].children.end(), idx );
                assert( it != data[parent].children.end() );
                data[parent].children.erase( it );
                toplevel.push_back( idx );
                break;
            }
            idx = parent;
            visited.emplace( idx );
        }
    }

    printf( "\nTop level messages: %i\nMissing messages (maybe crosspost): %i\nMalformed references: %i\nUnparsable date fields: %i (%i recovered)\nReference loops: %i\n", toplevel.size(), missing.size(), broken, baddate, recdate, loopcnt );

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

    printf( "\nCalculating total children counts...\n" );
    std::function<int(int)> Count;
    Count = [&data, &Count]( int idx ) {
        if( data[idx].childTotal != std::numeric_limits<uint32_t>::max() )
        {
            return data[idx].childTotal;
        }
        uint32_t cnt = 1;
        auto& children = data[idx].children;
        for( int j=0; j<children.size(); j++ )
        {
            if( data[children[j]].childTotal == std::numeric_limits<uint32_t>::max() )
            {
                cnt += Count( children[j] );
            }
            else
            {
                cnt += data[children[j]].childTotal;
            }
        }
        data[idx].childTotal = cnt;
        return cnt;
    };
    for( int i=0; i<size; i++ )
    {
        Count( i );
    }

    printf( "Saving...\n" );
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
        offset += fwrite( &data[i].childTotal, 1, sizeof( Message::childTotal ), cdata );
        uint32_t cnum = data[i].children.size();
        offset += fwrite( &cnum, 1, sizeof( cnum ), cdata );
        for( auto& v : data[i].children )
        {
            offset += fwrite( &v, 1, sizeof( v ), cdata );
        }
    }
    fclose( cdata );
    fclose( cmeta );

    printf( "%i/%i\n", size, size );

    delete[] data;
    return 0;
}
