#include <stdio.h>
#include <stdlib.h>

#include "Socket.hpp"

#include "../common/Filesystem.hpp"

int main( int argc, char** argv )
{
    if( argc != 4 )
    {
        fprintf( stderr, "USAGE: %s server group lastarticle\n\n", argv[0] );
        fprintf( stderr, "  Note: The first downloaded article will be lastarticle+1.\n" );
        exit( 1 );
    }
    if( Exists( argv[1] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    Socket sock;
    sock.Connect( argv[1] );

    enum { BufSize = 4 * 1024 };
    char buf[BufSize];

    auto len = sock.Recv( buf, BufSize );
    if( strncmp( buf, "200", 3 ) != 0 && strncmp( buf, "201", 3 ) != 0 )
    {
        fprintf( stderr, "%.*s", len, buf );
        exit( 1 );
    }



    sock.Send( "QUIT\r\n", 6 );
    sock.Recv( buf, BufSize );
    sock.Close();
}
