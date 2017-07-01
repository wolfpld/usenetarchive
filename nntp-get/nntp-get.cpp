#include <stdio.h>
#include <stdlib.h>

#include "../common/Filesystem.hpp"

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s group\n", argv[0] );
        exit( 1 );
    }
    if( Exists( argv[1] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }
}
