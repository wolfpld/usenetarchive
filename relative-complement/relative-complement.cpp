#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "../common/Filesystem.hpp"
#include "../common/HashSearch.hpp"
#include "../common/MessageView.hpp"
#include "../common/MetaView.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/StringCompress.hpp"

int main( int argc, char** argv )
{
    if( argc != 4 )
    {
        fprintf( stderr, "USAGE: %s destination data-source exclusion-source\n", argv[0] );
        exit( 1 );
    }
    if( Exists( argv[1] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }
    if( !Exists( argv[2] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }
    if( !Exists( argv[3] ) )
    {
        fprintf( stderr, "Exclusion directory doesn't exist.\n" );
        exit( 1 );
    }

    CreateDirStruct( argv[1] );

    std::string base1( argv[2] );
    base1 += "/";
    std::string base2( argv[3] );
    base2 += "/";
    std::string basedst( argv[1] );
    basedst += "/";

    const MessageView mview1( base1 + "meta", base1 + "data" );
    const MetaView<uint32_t, uint8_t> mid1( base1 + "midmeta", base1 + "middata" );
    const StringCompress compress1( base1 + "msgid.codebook" );
    const MessageView mview2( base2 + "meta", base2 + "data" );
    const HashSearch<uint8_t> hash2( base2 + "middata", base2 + "midhash", base2 + "midhashdata" );
    const StringCompress compress2( base2 + "msgid.codebook" );

    std::string metadstfn = basedst + "meta";
    std::string datadstfn = basedst + "data";

    FILE* meta3 = fopen( metadstfn.c_str(), "wb" );
    FILE* data3 = fopen( datadstfn.c_str(), "wb" );

    printf( "Src1 size: %zu. Src2 size: %zu.\n", mview1.Size(), mview2.Size() );
    fflush( stdout );

    uint32_t added = 0;
    uint32_t dupes = 0;
    uint32_t offset = 0;
    const auto size = mview1.Size();
    for( int i=0; i<size; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%zu\r", i, size );
            fflush( stdout );
        }

        uint8_t repack[2048];
        compress2.Repack( mid1[i], repack, compress1 );
        if( hash2.Search( repack ) >= 0 )
        {
            dupes++;
        }
        else
        {
            added++;
            const auto raw = mview1.Raw( i );
            fwrite( raw.ptr, 1, raw.compressedSize, data3 );
            RawImportMeta metaPacket = { offset, uint32_t( raw.size ), uint32_t( raw.compressedSize ) };
            fwrite( &metaPacket, 1, sizeof( RawImportMeta ), meta3 );
            offset += raw.compressedSize;
        }
    }

    printf( "\n%zu posts checked. %i unique, %i excluded.\n", size, added, dupes );

    fclose( meta3 );
    fclose( data3 );

    return 0;
}
