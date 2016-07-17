#include <algorithm>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "../common/FileMap.hpp"
#include "../common/LexiconTypes.hpp"
#include "../common/System.hpp"
#include "../common/TaskDispatch.hpp"

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

    const auto cpus = System::CPUCores();
    printf( "Running %i threads...\n", cpus );

    std::mutex mtx;
    TaskDispatch tasks( cpus );
    uint32_t start = 0;
    uint32_t inPass = ( size + cpus - 1 ) / cpus;
    uint32_t left = size;
    uint32_t cnt = 0;

    for( int i=0; i<cpus; i++ )
    {
        uint32_t todo = std::min( left, inPass );
        tasks.Queue( [data, lengths, start, &mtx, &cnt, size, &meta, &str, todo] () {
            for( uint32_t i=start; i<start+todo; i++ )
            {
                mtx.lock();
                auto c = cnt++;
                mtx.unlock();
                if( ( c & 0x3F ) == 0 )
                {
                    printf( "%i/%i\r", c, size );
                    fflush( stdout );
                }

                auto mp = meta + i;
                auto s = str + mp->str;

                for( uint32_t j=0; j<size; j++ )
                {
                    if( i == j ) continue;
                    auto mp2 = meta + j;
                    auto s2 = str + mp2->str;

                    if( lengths[i] == 3 )
                    {
                        if( lengths[j] == 3 && levenshtein_distance( s, lengths[i], s2, lengths[j] ) <= 1 )
                        {
                            data[i].push_back( mp2->str );
                        }
                    }
                    else
                    {
                        if( lengths[j] > 3 && levenshtein_distance( s, lengths[i], s2, lengths[j] ) <= 2 )
                        {
                            data[i].push_back( mp2->str );
                        }
                    }
                }
            }
        } );
        start += todo;
        left -= todo;
    }
    tasks.Sync();

    return 0;
}
