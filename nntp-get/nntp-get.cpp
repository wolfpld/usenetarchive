#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>

#include "Socket.hpp"

#include "../common/Filesystem.hpp"
#include "../common/ParseDate.hpp"
#include "../common/String.hpp"

enum { BufSize = 1024 * 1024 };
char buf[BufSize], buf2[BufSize];

std::vector<const char*> cache;
ParseDateStats stats;
uint32_t droppedMessages = 0;


static void Demangle( FILE* f, const char* buf )
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

static bool ReceiveMessage( Socket& sock, const std::string& dir, int article, time_t dateLimit )
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

    ptr = buf;
    while( *ptr++ != '\n' ) {}

    if( dateLimit != 0 )
    {
        auto src = ptr;
        auto dst = buf2;
        while( src < ptr + len )
        {
            if( *src != '\r' )
            {
                *dst++ = *src++;
            }
            else
            {
                src++;
            }
        }
        *dst = '\0';

        auto date = ParseDate( buf2, stats, cache );
        if( date < dateLimit )
        {
            droppedMessages++;
            return true;
        }
    }

    if( !Exists( dir ) )
    {
        CreateDirStruct( dir );
    }

    char tmp[1024];
    sprintf( tmp, "%s/%i", dir.c_str(), article );
    FILE* f = fopen( tmp, "wb" );
    Demangle( f, ptr );
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
    if( argc < 3 )
    {
        fprintf( stderr, "USAGE: %s server input [date-limit]\n\n", argv[0] );
        fprintf( stderr, "  Note: input should be a text file with one {group lastarticle} entry per line.\n" );
        fprintf( stderr, "  Note: The first downloaded article will be lastarticle+1.\n" );
        fprintf( stderr, "  Note: If date-limit is specified (yyyy-mm-dd), only articles newer than that date will be downloaded.\n" );
        exit( 1 );
    }

    FILE* in = fopen( argv[2], "r" );
    if( !in )
    {
        fprintf( stderr, "Cannot open %s!", argv[2] );
        exit( 1 );
    }
    std::vector<std::pair<std::string, int>> groups;
    std::string tmpstr;
    while( ReadLine( in, tmpstr ) )
    {
        auto s = tmpstr.c_str();
        auto ptr = s;
        while( *ptr != ' ' ) ptr++;
        groups.emplace_back( std::string( s, ptr ), atoi( ptr+1 ) );
    }
    fclose( in );

    time_t dateLimit = 0;
    if( argc > 3 )
    {
        struct tm tm = {};
        strptime( argv[3], "%Y-%m-%d", &tm );
        dateLimit = mktime( &tm );
        if( dateLimit == -1 )
        {
            fprintf( stderr, "Invalid date format!\n" );
            exit( 1 );
        }
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

    for( auto& v : groups )
    {
        sprintf( tmp, "GROUP %s\r\n", v.first.c_str() );
        sock.Send( tmp, strlen( tmp ) );
        len = sock.Recv( buf, BufSize );
        if( strncmp( buf, "211", 3 ) != 0 )
        {
            fprintf( stderr, "%.*s", len, buf );
            die( sock );
        }

        droppedMessages = 0;
        const int start = v.second + 1;
        int article = start;
        printf( "%s %i\r", v.first.c_str(), article );
        fflush( stdout );
        sprintf( tmp, "ARTICLE %i\r\n", article );
        sock.Send( tmp, strlen( tmp ) );
        if( !ReceiveMessage( sock, v.first.c_str(), article, dateLimit ) )
        {
            printf( "%s no new messages\n", v.first.c_str() );
        }
        else
        {
            for(;;)
            {
                article++;
                printf( "%s %i\r", v.first.c_str(), article );
                fflush( stdout );
                sock.Send( "NEXT\r\n", 6 );
                sock.Recv( buf, BufSize );
                if( strncmp( buf, "223", 3 ) != 0 )
                {
                    break;
                }
                sock.Send( "ARTICLE\r\n", 9 );
                if( !ReceiveMessage( sock, v.first, article, dateLimit ) )
                {
                    break;
                }
            }
            if( dateLimit == 0 )
            {
                printf( "%s %i..%i (+%i)\n", v.first.c_str(), start, article-1, article-start );
            }
            else
            {
                printf( "%s %i..%i (+%i, %i dropped)\n", v.first.c_str(), start, article-1, article-start-droppedMessages, droppedMessages );
            }
        }
    }

    sock.Send( "QUIT\r\n", 6 );
    sock.Recv( buf, BufSize );
    sock.Close();
}
