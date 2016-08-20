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

struct Stats
{
    uint32_t cnt;
    uint32_t lexdata;
    uint32_t lexhit;
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
    FileMap<LexiconMetaPacket> meta( base + "lexmeta" );
    FileMap<char> str( base + "lexstr" );
    FileMap<uint32_t> ldata( base + "lexdata" );
    FileMap<uint8_t> hits( base + "lexhit" );

    std::vector<std::pair<const char*, Stats>> data;

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
        uint32_t ld = 0;
        uint32_t lh = 0;

        auto dptr = ldata + ( mp->data / sizeof( uint32_t ) );
        ld += mp->dataSize;
        for( uint32_t j=0; j<mp->dataSize; j++ )
        {
            dptr++;
            auto hptr = hits + ( *dptr++ / sizeof( uint8_t ) );
            auto hnum = *hptr++;
            lh += hnum + 1;
            cnt += hnum;
            totalSize += hnum;
            for( uint8_t k=0; k<hnum; k++ )
            {
                sizes[LexiconDecodeType(*hptr++)]++;
            }
        }

        data.emplace_back( s, Stats { cnt, ld, lh } );
    }

    printf( "Total words: %" PRIu64 "\n", totalSize );
    for( int i=0; i<6; i++ )
    {
        printf( "Lexicon category %s: %" PRIu64 " hits (%.1f%%)\n", LexiconNames[i], sizes[i], sizes[i] * 100.f / totalSize );
    }

    std::sort( data.begin(), data.end(), [] ( const auto& lhs, const auto& rhs ) { return lhs.second.cnt > rhs.second.cnt; } );

    uint32_t dt = 0;
    uint32_t ht = 0;
    for( auto& v : data )
    {
        dt += v.second.lexdata * sizeof( uint32_t ) * 2;
        ht += v.second.lexhit;
        fprintf( stderr, "%i\t%s\t(%i B data, %i B hits)\n", v.second.cnt, v.first, v.second.lexdata * sizeof( uint32_t ) * 2, v.second.lexhit );
    }

    printf( "Total %iKB data, %iKB hits\n", dt / 1024, ht / 1024 );

    return 0;
}
