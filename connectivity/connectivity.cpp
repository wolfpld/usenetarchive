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
#include <vector>

#include "../contrib/martinus/robin_hood.h"
#include "../common/Filesystem.hpp"
#include "../common/HashSearch.hpp"
#include "../common/MessageView.hpp"
#include "../common/ParseDate.hpp"
#include "../common/ReferencesParent.hpp"
#include "../common/StringCompress.hpp"

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
    const HashSearch<uint8_t> hash( base + "middata", base + "midhash", base + "midhashdata" );
    const StringCompress compress( base + "msgid.codebook" );

    const auto size = mview.Size();

    printf( "Building graph...\n" );
    fflush( stdout );

    unsigned int broken = 0;
    std::vector<uint32_t> toplevel;
    auto data = new Message[size];

    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%zu\r", i, size );
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

    std::vector<const char*> cache;
    ParseDateStats stats = {};
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%zu\r", i, size );
            fflush( stdout );
        }
        data[i].epoch = ParseDate( mview[i], stats, cache );
    }

    unsigned int loopcnt = 0;
    robin_hood::unordered_flat_set<uint32_t> visited;
    printf( "\nFixing loops...\n" );
    fflush( stdout );
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%zu\r", i, size );
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

    printf( "\nTop level messages: %zu\nMalformed references: %i\nUnparsable date fields: %i (%i recovered)\nTime traveling mesages: %i\nReference loops: %i\n", toplevel.size(), broken, stats.baddate, stats.recdate, stats.timetravel, loopcnt );

    printf( "Sorting top level...\n" );
    fflush( stdout );
    Sort( toplevel, data );
    printf( "Sorting children...\n" );
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0xFFF ) == 0 )
        {
            printf( "%i/%zu\r", i, size );
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
            printf( "%i/%zu\r", i, size );
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

    printf( "%zu/%zu\n", size, size );

    delete[] data;
    return 0;
}
