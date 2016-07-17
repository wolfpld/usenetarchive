#include <algorithm>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "../common/FileMap.hpp"
#include "../common/LexiconTypes.hpp"

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
    FileMap<uint32_t> ldata( base + "lexdata" );
    FileMap<uint16_t> hits( base + "lexhit" );

    std::vector<std::pair<const char*, uint32_t>> data;

    const auto size = meta.DataSize();
    uint32_t sizes[6] = {};
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto mp = meta + i;
        auto s = str + mp->str;
        uint32_t cnt = 0;

        auto dptr = ldata + ( mp->data / sizeof( uint32_t ) );
        for( uint32_t j=0; j<mp->dataSize; j++ )
        {
            dptr++;
            auto hptr = hits + ( *dptr++ / sizeof( uint16_t ) );
            auto hnum = *hptr++;
            for( uint16_t k=0; k<hnum; k++ )
            {
                cnt++;
            }
        }

        data.emplace_back( s, cnt );
    }

    std::sort( data.begin(), data.end(), [] ( const auto& lhs, const auto& rhs ) { return lhs.second > rhs.second; } );

    for( auto& v : data )
    {
        fprintf( stderr, "%i\t%s\n", v.second, v.first );
    }

    return 0;
}
