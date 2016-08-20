#include <algorithm>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "../common/FileMap.hpp"
#include "../common/LexiconTypes.hpp"

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s directory\n", argv[0] );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );
    FileMap<LexiconMetaPacket> meta( base + "lexmeta" );
    FileMap<char> str( base + "lexstr" );
    FileMap<uint32_t> ldata( base + "lexdata" );
    FileMap<uint8_t> hits( base + "lexhit" );

    std::vector<std::pair<const char*, uint32_t>> data;

    const auto size = meta.DataSize();
    uint64_t sizes[6] = {};
    uint64_t totalSize = 0;
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
            auto hptr = hits + ( *dptr++ / sizeof( uint8_t ) );
            auto hnum = *hptr++;
            for( uint8_t k=0; k<hnum; k++ )
            {
                cnt++;
                totalSize++;
                sizes[LexiconDecodeType(*hptr++)]++;
            }
        }

        data.emplace_back( s, cnt );
    }

    printf( "Total words: %" PRIu64 "\n", totalSize );
    for( int i=0; i<6; i++ )
    {
        printf( "Lexicon category %s: %" PRIu64 " hits (%.1f%%)\n", LexiconNames[i], sizes[i], sizes[i] * 100.f / totalSize );
    }

    std::sort( data.begin(), data.end(), [] ( const auto& lhs, const auto& rhs ) { return lhs.second > rhs.second; } );

    for( auto& v : data )
    {
        fprintf( stderr, "%i\t%s\n", v.second, v.first );
    }

    return 0;
}
