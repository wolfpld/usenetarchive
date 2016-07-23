#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/MessageView.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"

int main( int argc, char** argv )
{
    if( argc != 4 )
    {
        fprintf( stderr, "USAGE: %s newsgroup source destination\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[2] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[3] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    CreateDirStruct( argv[3] );

    std::string base = argv[2];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const auto size = mview.Size();

    std::string dbase = argv[3];
    dbase.append( "/" );
    std::string dmetafn = dbase + "meta";
    std::string ddatafn = dbase + "data";

    FILE* dmeta = fopen( dmetafn.c_str(), "wb" );
    FILE* ddata = fopen( ddatafn.c_str(), "wb" );

    const auto matchlen = strlen( argv[1] );

    ExpandingBuffer eb;
    uint32_t cntgood = 0;
    uint64_t savec = 0, saveu = 0;
    uint64_t offset = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto buf = post;

        while( strnicmpl( buf, "newsgroups: ", 12 ) != 0 )
        {
            buf++;
            while( *buf++ != '\n' ) {}
        }
        buf += 12;
        auto end = buf;
        bool good = false;
        while( *end != '\n' )
        {
            while( *end == ' ' || *end == ',' ) end++;
            buf = end;
            while( *end != '\n' && *end != ',' ) end++;
            if( end-buf == matchlen )
            {
                if( strncmp( buf, argv[1], matchlen ) == 0 )
                {
                    const auto raw = mview.Raw( i );
                    fwrite( raw.ptr, 1, raw.compressedSize, ddata );

                    RawImportMeta metaPacket = { offset, raw.size, raw.compressedSize };
                    fwrite( &metaPacket, 1, sizeof( RawImportMeta ), dmeta );
                    offset += raw.compressedSize;

                    cntgood++;
                    good = true;
                    break;
                }
            }
        }
        if( !good )
        {
            const auto raw = mview.Raw( i );
            savec += raw.compressedSize;
            saveu += raw.size;
        }
    }

    fclose( dmeta );
    fclose( ddata );

    printf( "Processed %i messages. Valid newsgroup: %i, bogus messages: %i\n", size, cntgood, size - cntgood );
    printf( "Saved %i KB (uncompressed), %i KB (compressed)\n", saveu / 1024, savec / 1024 );

    return 0;
}
