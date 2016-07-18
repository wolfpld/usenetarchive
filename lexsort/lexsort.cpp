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

struct DataStruct
{
    uint32_t id;
    uint32_t offset;
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

    DataStruct* data;
    uint8_t* hits;
    uint32_t datasize, hitssize;
    {
        FileMap<DataStruct> mdata( base + "lexdata" );
        FileMap<uint8_t> mhits( base + "lexhit" );

        datasize = mdata.DataSize();
        hitssize = mhits.DataSize();

        data = new DataStruct[datasize];
        hits = new uint8_t[hitssize];

        memcpy( data, mdata, mdata.Size() );
        memcpy( hits, mhits, mhits.Size() );
    }

    const auto size = meta.DataSize();
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto mp = meta + i;
        auto dptr = data + ( mp->data / sizeof( DataStruct ) );
        auto dsize = mp->dataSize;
        std::sort( dptr, dptr + dsize, [] ( const auto& l, const auto& r ) { return ( l.id & LexiconPostMask ) < ( r.id & LexiconPostMask ); } );

        for( int i=0; i<dsize; i++ )
        {
            auto hptr = hits + dptr[i].offset;
            auto hnum = *hptr++;
            if( hnum > 1 )
            {
                std::sort( hptr, hptr + hnum, [] ( const auto& l, const auto& r ) { return LexiconHitRank( l ) > LexiconHitRank( r ); } );
            }
        }
    }

    printf( "\n" );

    FILE* fdata = fopen( ( base + "lexdata" ).c_str(), "wb" );
    FILE* fhits = fopen( ( base + "lexhit" ).c_str(), "wb" );

    fwrite( data, 1, datasize * sizeof( DataStruct ), fdata );
    fwrite( hits, 1, hitssize * sizeof( uint8_t ), fhits );

    fclose( fdata );
    fclose( fhits );

    delete[] data;
    delete[] hits;

    return 0;
}