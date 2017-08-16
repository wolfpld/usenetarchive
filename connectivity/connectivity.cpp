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
#include "../common/ReferencesParent.hpp"
#include "../common/String.hpp"
#include "../common/StringCompress.hpp"

enum { TimeTravelLimit = 60*60*24*7 };      // one week

extern "C" { time_t parsedate_rfc5322_lax(const char *date); }

struct Message
{
    uint32_t epoch = 0;
    int32_t parent = -1;
    uint32_t childTotal = std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> children;
};

struct TimeGroup
{
    uint32_t timestamp;
    uint32_t count;
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
    const HashSearch<uint8_t> hash( base + "middata", base + "midhash", base + "midhashdata" );
    const StringCompress compress( base + "msgid.codebook" );

    const auto size = mview.Size();

    printf( "Building graph...\n" );
    fflush( stdout );

    unsigned int broken = 0;
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

        char tmp[1024];
        auto parent = GetParentFromReferences( post, compress, hash, tmp );
        if( parent == -2 )
        {
            broken++;
        }
        if( parent < 0 )
        {
            toplevel.push_back( i );
        }
        else
        {
            data[i].parent = parent;
            data[parent].children.emplace_back( i );
        }
    }

    printf( "\nRetrieving timestamps...\n" );
    fflush( stdout );

    std::vector<const char*> received;
    unsigned int baddate = 0;
    unsigned int recdate = 0;
    unsigned int timetravel = 0;
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

        buf = FindOptionalHeader( post, "nntp-posting-date: ", 19 );
        if( *buf != '\n' )
        {
            buf += 19;
            if( *buf != '\n' )
            {
                received.emplace_back( buf );
            }
        }
        buf = FindOptionalHeader( post, "injection-date: ", 16 );
        if( *buf != '\n' )
        {
            buf += 16;
            if( *buf != '\n' )
            {
                received.emplace_back( buf );
            }
        }

        time_t recvdate = -1;
        if( received.size() == 1 )
        {
            recvdate = parsedate_rfc5322_lax( received[0] );
        }
        else if( received.size() > 1 )
        {
            std::vector<uint32_t> timestamps;
            timestamps.reserve( received.size() );
            for( auto& v : received )
            {
                auto ts = parsedate_rfc5322_lax( v );
                if( ts != -1 )
                {
                    timestamps.emplace_back( ts );
                }
            }
            if( timestamps.size() == 1 )
            {
                recvdate = timestamps[0];
            }
            else if( timestamps.size() > 1 )
            {
                assert( timestamps.size() < std::numeric_limits<uint16_t>::max() );
                std::vector<uint16_t> sort;
                sort.reserve( timestamps.size() );
                for( int i=0; i<timestamps.size(); i++ )
                {
                    sort.emplace_back( i );
                }
                std::sort( sort.begin(), sort.end(), [&timestamps] ( const auto& l, const auto& r ) { return timestamps[l] < timestamps[r]; } );

                std::vector<TimeGroup> groups;
                groups.emplace_back( TimeGroup { timestamps[sort[0]], 1 } );
                for( int i=1; i<timestamps.size(); i++ )
                {
                    assert( timestamps[sort[i]] >= groups.back().timestamp );
                    if( timestamps[sort[i]] - groups.back().timestamp > TimeTravelLimit )
                    {
                        groups.emplace_back( TimeGroup { timestamps[sort[i]], 1 } );
                    }
                    else
                    {
                        groups.back().count++;
                    }
                }
                if( groups.size() > 1 )
                {
                    std::sort( groups.begin(), groups.end(), [] ( const auto& l, const auto& r ) {
                        if( l.count == r.count )
                        {
                            return l.timestamp < r.timestamp;
                        }
                        else
                        {
                            return l.count > r.count;
                        }
                    } );
                }
                recvdate = groups[0].timestamp;
            }
        }

        time_t date = -1;
        buf = FindOptionalHeader( post, "date: ", 6 );
        if( *buf == '\n' )
        {
            buf = FindOptionalHeader( post, "date:\t", 6 );
        }
        if( *buf != '\n' )
        {
            buf += 6;
            auto end = buf;
            while( *end != '\n' && *end != '\0' ) end++;
            if( *end == '\n' )
            {
                const auto size = end - buf;
                memcpy( tmp, buf, size );
                tmp[size] = '\0';
                for( int i=0; i<size; i++ )
                {
                    if( tmp[i] == '-' ) tmp[i] = ' ';
                }
                date = parsedate_rfc5322_lax( tmp );
            }
        }

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
        else if( recvdate != -1 )
        {
            if( abs( date - recvdate ) > TimeTravelLimit )
            {
                timetravel++;
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

    printf( "\nTop level messages: %i\nMalformed references: %i\nUnparsable date fields: %i (%i recovered)\nTime traveling mesages: %i\nReference loops: %i\n", toplevel.size(), broken, baddate, recdate, timetravel, loopcnt );

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
