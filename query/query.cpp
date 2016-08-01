#include <chrono>
#include <memory>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "../contrib/linenoise-ng/linenoise.h"

#include "../common/Filesystem.hpp"
#include "../libuat/Archive.hpp"

void PrintHelp()
{
    printf( "child msgid   - view message's children\n" );
    printf( "childi idx    - view message's children\n" );
    printf( "date msgid    - view message's date\n" );
    printf( "datei idx     - view message's date\n" );
    printf( "desc          - show descriptions\n" );
    printf( "from msgid    - view from: field\n" );
    printf( "fromi idx     - view from: field\n" );
    printf( "info          - archive info\n" );
    printf( "parent msgid  - view message's parent\n" );
    printf( "parenti idx   - view message's parent\n" );
    printf( "search query  - search archive\n" );
    printf( "subject msgid - view subject: field\n" );
    printf( "subjecti idx  - view subject: field\n" );
    printf( "timechart     - print time chart\n" );
    printf( "toplevel      - list toplevel messages\n" );
    printf( "view msgid    - view message with given message id\n" );
    printf( "viewi idx     - view message of given idx\n" );
}

void Info( const Archive& archive )
{
    printf( "Number of messages: %i\n", archive.NumberOfMessages() );
    printf( "Number of toplevel messages: %i\n", archive.NumberOfTopLevel() );
}

int main( int argc, char** argv )
{
    if( argc < 2 )
    {
        fprintf( stderr, "USAGE: %s archive\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Archive doesn't exist.\n" );
        exit( 1 );
    }

    std::unique_ptr<Archive> archive( Archive::Open( argv[1] ) );
    if( !archive )
    {
        fprintf( stderr, "Cannot open archive!\n" );
        exit( 1 );
    }

    printf( "Usenet archive %s opened.\n", argv[1] );
    Info( *archive );

    while( char* cmd = linenoise( "\x1b[1;32mcmd>\x1b[0m " ) )
    {
        if( strncmp( cmd, "viewi ", 6 ) == 0 )
        {
            int idx = atoi( cmd+6 );
            auto msg = archive->GetMessage( idx );
            if( msg )
            {
                printf( "%s\n", msg );
            }
            else
            {
                printf( "Invalid message index (max %i).\n", archive->NumberOfMessages() );
            }
        }
        else if( strcmp( cmd, "toplevel" ) == 0 )
        {
            auto view = archive->GetTopLevel();
            for( uint64_t i=0; i<view.size; i++ )
            {
                printf( "%i\n", view.ptr[i] );
            }
        }
        else if( strcmp( cmd, "info" ) == 0 )
        {
            Info( *archive );
        }
        else if( strncmp( cmd, "view ", 5 ) == 0 )
        {
            auto msg = archive->GetMessage( cmd+5 );
            if( msg )
            {
                printf( "%s\n", msg );
            }
            else
            {
                printf( "Invalid message id.\n" );
            }
        }
        else if( strncmp( cmd, "parent ", 7 ) == 0 )
        {
            auto parent = archive->GetParent( cmd+7 );
            if( parent >= 0 )
            {
                printf( "Parent: %i\n", parent );
            }
            else
            {
                printf( "No parent.\n" );
            }
        }
        else if( strncmp( cmd, "parenti ", 8 ) == 0 )
        {
            auto parent = archive->GetParent( atoi( cmd+8 ) );
            if( parent >= 0 )
            {
                printf( "Parent: %i\n", parent );
            }
            else
            {
                printf( "No parent.\n" );
            }
        }
        else if( strncmp( cmd, "child ", 6 ) == 0 )
        {
            auto children = archive->GetChildren( cmd+6 );
            for( uint64_t i=0; i<children.size; i++ )
            {
                printf( "%i\n", children.ptr[i] );
            }
        }
        else if( strncmp( cmd, "childi ", 7 ) == 0 )
        {
            auto children = archive->GetChildren( atoi( cmd+7 ) );
            for( uint64_t i=0; i<children.size; i++ )
            {
                printf( "%i\n", children.ptr[i] );
            }
        }
        else if( strncmp( cmd, "date ", 5 ) == 0 )
        {
            auto date = archive->GetDate( cmd+6 );
            time_t t = { date };
            printf( "%s\n", asctime( localtime( &t ) ) );
        }
        else if( strncmp( cmd, "datei ", 6 ) == 0 )
        {
            auto date = archive->GetDate( atoi( cmd+7 ) );
            time_t t = { date };
            printf( "%s\n", asctime( localtime( &t ) ) );
        }
        else if( strncmp( cmd, "from ", 5 ) == 0 )
        {
            auto data = archive->GetFrom( cmd+5 );
            if( data )
            {
                printf( "%s\n", data );
            }
            else
            {
                printf( "Invalid message id.\n" );
            }
        }
        else if( strncmp( cmd, "fromi ", 6 ) == 0 )
        {
            auto data = archive->GetFrom( atoi( cmd+6 ) );
            if( data )
            {
                printf( "%s\n", data );
            }
            else
            {
                printf( "Invalid message id.\n" );
            }
        }
        else if( strncmp( cmd, "subject ", 8 ) == 0 )
        {
            auto data = archive->GetSubject( cmd+8 );
            if( data )
            {
                printf( "%s\n", data );
            }
            else
            {
                printf( "Invalid message id.\n" );
            }
        }
        else if( strncmp( cmd, "subjecti ", 9 ) == 0 )
        {
            auto data = archive->GetSubject( atoi( cmd+9 ) );
            if( data )
            {
                printf( "%s\n", data );
            }
            else
            {
                printf( "Invalid message id.\n" );
            }
        }
        else if( strncmp( cmd, "search ", 7 ) == 0 )
        {
            auto t0 = std::chrono::high_resolution_clock::now();
            auto data = archive->Search( cmd+7 );
            auto t1 = std::chrono::high_resolution_clock::now();
            printf( "Query time %fms.\n", std::chrono::duration_cast<std::chrono::microseconds>( t1 - t0 ).count() / 1000.f );
            printf( "Found %i messages.\n", data.size() );
            if( !data.empty() )
            {
                bool first = true;
                for( auto& v : data )
                {
                    printf( "%s%i (%.2f)", first ? "" : ", ", v.postid, v.rank );
                    first = false;
                }
                printf( "\n" );
            }
        }
        else if( strcmp( cmd, "timechart" ) == 0 )
        {
            auto tc = archive->TimeChart();
            for( auto& v : tc )
            {
                printf( "%s,%i\n", v.first.c_str(), v.second );
            }
        }
        else if( strcmp( cmd, "exit" ) == 0 || strcmp( cmd, "quit" ) == 0 )
        {
            return 0;
        }
        else if( strcmp( cmd, "desc" ) == 0 )
        {
            auto d1 = archive->GetShortDescription();
            auto d2 = archive->GetLongDescription();

            if( d1.first )
            {
                printf( "%.*s\n", d1.second, d1.first );
            }
            else
            {
                printf( "No short description.\n" );
            }
            if( d2.first )
            {
                printf( "\n%.*s\n", d2.second, d2.first );
            }
            else
            {
                printf( "No long description.\n" );
            }
        }
        else
        {
            PrintHelp();
        }
        free( cmd );
    }

    return 0;
}
