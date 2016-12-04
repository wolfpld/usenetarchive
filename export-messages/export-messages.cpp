#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "../common/Filesystem.hpp"
#include "../common/MessageView.hpp"
#include "../common/MetaView.hpp"
#include "../common/String.hpp"

int main( int argc, char** argv )
{
    if( argc != 3 )
    {
        fprintf( stderr, "USAGE: %s source destination\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[2] ) )
    {
        fprintf( stderr, "Destination directory exists.\n" );
        exit( 1 );
    }

    CreateDirStruct( argv[2] );

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const MetaView<uint32_t, char> mid( base + "midmeta", base + "middata" );

    const auto size = mview.Size();
    for( int i=0; i<size; i++ )
    {
        if( ( i & 0x1FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto raw = mview.Raw( i );
        auto ptr = mview[i];

        char buf[1024];
        sprintf( buf, "%s/%s", argv[2], mid[i] );
        FILE* f = fopen( buf, "wb" );
        if( !f )
        {
            sprintf( buf, "%s/%i", argv[2], i );
            f = fopen( buf, "wb" );
        }
        fwrite( ptr, 1, raw.size, f );
        fclose( f );
    }

    printf( "\n" );

    return 0;
}
