#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "../libuat/Archive.hpp"

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s archive\n", argv[0] );
        exit( 1 );
    }

    auto archive = Archive::Open( argv[1] );

    return 0;
}
