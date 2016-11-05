#include <stdio.h>
#include <stdlib.h>

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "Usage: %s archive-name.usenet\n", argv[0] );
        return 1;
    }

    return 0;
}
