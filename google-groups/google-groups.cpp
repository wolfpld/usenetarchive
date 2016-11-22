// Based on https://github.com/henryk/gggd

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <curl/curl.h>

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
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }
    if( curl_global_init( CURL_GLOBAL_ALL ) != 0 )
    {
        fprintf( stderr, "Cannot initialize libcurl.\n" );
        exit( 1 );
    }



    //CreateDirStruct( argv[1] );

    std::string base = argv[1];
    base.append( "/" );

    return 0;
}
