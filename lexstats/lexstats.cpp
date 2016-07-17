#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "../common/FileMap.hpp"

struct MetaPacket
{
    uint32_t str;
    uint32_t data;
    uint32_t dataSize;
};

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s directory\n", argv[0] );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );
    FileMap<MetaPacket> meta( base + "lexmeta" );
    FileMap<char> str( base + "lexstr" );

    std::vector<std::pair<const char*, uint32_t>> data;

    const auto size = meta.DataSize();
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto mp = meta + i;
        auto s = str + mp->str;

        data.emplace_back( s, mp->dataSize );
    }

    std::sort( data.begin(), data.end(), [] ( const auto& lhs, const auto& rhs ) { return lhs.second > rhs.second; } );

    for( auto& v : data )
    {
        fprintf( stderr, "%i\t%s\n", v.second, v.first );
    }

    return 0;
}
