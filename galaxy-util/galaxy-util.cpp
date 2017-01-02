#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../common/Filesystem.hpp"

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s directory\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Destination directory doesn't exist.\n" );
        exit( 1 );
    }


    return 0;
}
