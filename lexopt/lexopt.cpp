#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unordered_set>
#include <vector>

#include "../common/CharUtil.hpp"
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
    std::string fnmeta = base + "lexmeta";
    std::string fnstr = base + "lexstr";

    char* strbuf;
    LexiconMetaPacket* metabuf;
    uint32_t strsize;
    uint32_t metasize;

    {
        FileMap<LexiconMetaPacket> meta( fnmeta );
        FileMap<char> str( fnstr );

        metasize = meta.DataSize();
        metabuf = new LexiconMetaPacket[metasize];
        memcpy( metabuf, meta, meta.Size() );

        strsize = str.DataSize();
        strbuf = new char[strsize];
        memcpy( strbuf, str, str.Size() );
    }

    uint32_t* strlength = new uint32_t[metasize];
    uint32_t* order = new uint32_t[metasize];
    for( int i=0; i<metasize; i++ )
    {
        strlength[i] = strlen( strbuf + metabuf[i].str );
        order[i] = i;
    }

    std::sort( order, order + metasize, [&strlength]( const auto& l, const auto& r ) { return strlength[l] > strlength[r]; } );

    char* buf = new char[strsize];
    buf[0] = '\0';
    std::unordered_set<const char*, CharUtil::Hasher, CharUtil::Comparator> avail;

    unsigned int savings = 0;
    uint32_t offset = 1;
    for( int i=0; i<metasize; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, metasize );
            fflush( stdout );
        }

        auto idx = order[i];
        auto strptr = strbuf + metabuf[idx].str;
        auto it = avail.find( strptr );
        if( it == avail.end() )
        {
            memcpy( buf + offset, strptr, strlength[idx] + 1 );
            metabuf[idx].str = offset;
            // don't include full word
            for( int j=1; j<strlength[idx]; j++ )
            {
                if( avail.find( buf + offset + j ) == avail.end() )
                {
                    avail.emplace( buf + offset + j );
                }
                else
                {
                    // rest of words are already present
                    break;
                }
            }
            offset += strlength[idx] + 1;
        }
        else
        {
            savings += strlength[idx];
            metabuf[idx].str = *it - buf;
        }
    }

    FILE* fstr = fopen( fnstr.c_str(), "wb" );
    fwrite( buf, 1, offset, fstr );
    fclose( fstr );

    FILE* fmeta = fopen( fnmeta.c_str(), "wb" );
    fwrite( metabuf, 1, sizeof( LexiconMetaPacket ) * metasize, fmeta );
    fclose( fmeta );

    printf( "\nSavings: %i KB, data size: %i KB\n", savings / 1024, offset / 1024 );

    return 0;
}
