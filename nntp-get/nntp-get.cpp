#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Socket.hpp"

#include "../common/Filesystem.hpp"

enum { BufSize = 1024 * 1024 };
char buf[BufSize];

static void Demangle( FILE* f, const char* buf, int size )
{
    auto ptr = buf;
    for(;;)
    {
        while( memcmp( ptr, "\r\n.", 3 ) != 0 ) ptr++;
        if( memcmp( ptr, "\r\n.\r\n", 5 ) == 0 )
        {
            fwrite( buf, 1, ptr - buf + 2, f );
            return;
        }
        fwrite( buf, 1, ptr - buf + 2, f );
        ptr += 3;
        buf = ptr;
    }
}

static bool ReceiveMessage( Socket& sock, const char* dir, int article )
{
    auto ptr = buf;
    auto size = sock.Recv( ptr, BufSize - ( ptr - buf ) );
    if( strncmp( buf, "220", 3 ) != 0 )
    {
        return false;
    }
    for(;;)
    {
        auto test = ptr;
        ptr += size;
        if( memcmp( test + size - 5, "\r\n.\r\n", 5 ) == 0 ) break;
        size = sock.Recv( ptr, BufSize - ( ptr - buf ) );
    }
    int len = ptr - buf;
    if( len == BufSize )
    {
        fprintf( stderr, "Message size greater than %i! Aborting.\n", BufSize );
        exit( 1 );
    }

    static bool dirDone = false;
    if( !dirDone )
    {
        dirDone = true;
        CreateDirStruct( dir );
    }

    ptr = buf;
    while( *ptr++ != '\n' ) {}

    char tmp[1024];
    sprintf( tmp, "%s/%i", dir, article );
    FILE* f = fopen( tmp, "wb" );
    Demangle( f, ptr, len - ( ptr - buf ) );
    fclose( f );

    return true;
}

static void die( Socket& sock )
{
    sock.Send( "QUIT\r\n", 6 );
    sock.Recv( buf, BufSize );
    sock.Close();
    exit( 1 );
}

int main( int argc, char** argv )
{
    if( argc != 4 )
    {
        fprintf( stderr, "USAGE: %s server group lastarticle\n\n", argv[0] );
        fprintf( stderr, "  Note: The first downloaded article will be lastarticle+1.\n" );
        exit( 1 );
    }
    if( Exists( argv[2] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    Socket sock;
    sock.Connect( argv[1] );

    char tmp[1024];

    auto len = sock.Recv( buf, BufSize );
    if( strncmp( buf, "200", 3 ) != 0 && strncmp( buf, "201", 3 ) != 0 )
    {
        fprintf( stderr, "%.*s", len, buf );
        die( sock );
    }

    sprintf( tmp, "GROUP %s\r\n", argv[2] );
    sock.Send( tmp, strlen( tmp ) );
    len = sock.Recv( buf, BufSize );
    if( strncmp( buf, "211", 3 ) != 0 )
    {
        fprintf( stderr, "%.*s", len, buf );
        die( sock );
    }

    int article = atoi( argv[3] ) + 1;
    printf( "%i\r", article );
    fflush( stdout );
    sprintf( tmp, "ARTICLE %i\r\n", article );
    sock.Send( tmp, strlen( tmp ) );
    if( !ReceiveMessage( sock, argv[2], article ) )
    {
        fprintf( stderr, "No such article\n" );
        die( sock );
    }

    for(;;)
    {
        article++;
        printf( "%i\r", article );
        fflush( stdout );
        sock.Send( "NEXT\r\n", 6 );
        sock.Recv( buf, BufSize );
        if( strncmp( buf, "223", 3 ) != 0 )
        {
            break;
        }
        sock.Send( "ARTICLE\r\n", 9 );
        if( !ReceiveMessage( sock, argv[2], article ) )
        {
            break;
        }
    }

    sock.Send( "QUIT\r\n", 6 );
    sock.Recv( buf, BufSize );
    sock.Close();

    printf( "\n" );
}
