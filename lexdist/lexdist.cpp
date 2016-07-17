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

// https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C.2B.2B
static unsigned int levenshtein_distance( const char* s1, const unsigned int len1, const char* s2, const unsigned int len2 )
{
    static thread_local unsigned int _col[128], _prevCol[128];
    unsigned int *col = _col, *prevCol = _prevCol;

    for( unsigned int i = 0; i < len2+1; i++ )
    {
        prevCol[i] = i;
    }
    for( unsigned int i = 0; i < len1; i++ )
    {
        col[0] = i+1;
        for( unsigned int j = 0; j < len2; j++ )
        {
            col[j+1] = std::min( { prevCol[1 + j] + 1, col[j] + 1, prevCol[j] + (s1[i]==s2[j] ? 0 : 1) } );
        }
        std::swap( col, prevCol );
    }
    return prevCol[len2];
}

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

    const auto size = meta.DataSize();
    auto data = new std::vector<uint32_t>[size];
    auto lengths = new unsigned int[size];
    for( uint32_t i=0; i<size; i++ )
    {
        auto mp = meta + i;
        auto s = str + mp->str;
        lengths[i] = strlen( s );
    }
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x1F ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto mp = meta + i;
        auto s = str + mp->str;

        for( uint32_t j=0; j<size; j++ )
        {
            if( i == j ) continue;
            auto mp2 = meta + j;
            auto s2 = str + mp2->str;

            if( levenshtein_distance( s, lengths[i], s2, lengths[j] ) <= 2 )
            {
                data[i].push_back( mp2->str );
            }
        }
    }

    return 0;
}
