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

    LexiconDataPacket* data;
    uint8_t* hits;
    uint32_t datasize, hitssize;
    {
        FileMap<LexiconDataPacket> mdata( base + "lexdata" );
        FileMap<uint8_t> mhits( base + "lexhit" );

        datasize = mdata.DataSize();
        hitssize = mhits.DataSize();

        data = new LexiconDataPacket[datasize];
        hits = new uint8_t[hitssize];

        memcpy( data, mdata, mdata.Size() );
        memcpy( hits, mhits, mhits.Size() );
    }

    const auto size = meta.DataSize();
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%zu\r", i, size );
            fflush( stdout );
        }

        auto mp = meta + i;
        auto dptr = data + ( mp->data / sizeof( LexiconDataPacket ) );
        auto dsize = mp->dataSize;
        std::sort( dptr, dptr + dsize, [] ( const auto& l, const auto& r ) { return ( l.postid & LexiconPostMask ) < ( r.postid & LexiconPostMask ); } );

        for( int i=0; i<dsize; i++ )
        {
            uint8_t hnum = dptr[i].hitoffset >> LexiconHitShift;
            uint8_t* hptr;
            if( hnum == 0 )
            {
                hptr = hits + ( dptr[i].hitoffset & LexiconHitOffsetMask );
                hnum = *hptr++;
            }
            else
            {
                hptr = (uint8_t*)&dptr[i].hitoffset;
            }
            if( hnum > 1 )
            {
                std::sort( hptr, hptr + hnum, [] ( const auto& l, const auto& r ) { return LexiconHitRank( l ) > LexiconHitRank( r ); } );
            }
        }
    }

    printf( "\n" );

    FILE* fdata = fopen( ( base + "lexdata" ).c_str(), "wb" );
    FILE* fhits = fopen( ( base + "lexhit" ).c_str(), "wb" );

    fwrite( data, 1, datasize * sizeof( LexiconDataPacket ), fdata );
    fwrite( hits, 1, hitssize * sizeof( uint8_t ), fhits );

    fclose( fdata );
    fclose( fhits );

    delete[] data;
    delete[] hits;

    return 0;
}
