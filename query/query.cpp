#include <chrono>
#include <memory>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "../common/Filesystem.hpp"
#include "../libuat/Archive.hpp"

void PrintHelp()
{
    printf( "  child msgid   - view message's children\n" );
    printf( "  childi idx    - view message's children\n" );
    printf( "  date msgid    - view message's date\n" );
    printf( "  datei idx     - view message's date\n" );
    printf( "  desc          - show descriptions\n" );
    printf( "  from msgid    - view from: field\n" );
    printf( "  fromi idx     - view from: field\n" );
    printf( "  info          - archive info\n" );
    printf( "  parent msgid  - view message's parent\n" );
    printf( "  parenti idx   - view message's parent\n" );
    printf( "  search query  - search archive\n" );
    printf( "  subject msgid - view subject: field\n" );
    printf( "  subjecti idx  - view subject: field\n" );
    printf( "  timechart     - print time chart\n" );
    printf( "  toplevel      - list toplevel messages\n" );
    printf( "  view msgid    - view message with given message id\n" );
    printf( "  viewi idx     - view message of given idx\n" );
    printf( "  idx msgid     - get index of given msgid\n" );
}

void Info( const Archive& archive )
{
    printf( "Number of messages: %i\n", archive.NumberOfMessages() );
    printf( "Number of toplevel messages: %i\n", archive.NumberOfTopLevel() );
}

void BadArg()
{
    fprintf( stderr, "Missing argument!\n" );
    exit( 1 );
}

int main( int argc, char** argv )
{
    if( argc < 2 )
    {
        printf( "USAGE: %s archive [command]\n\n", argv[0] );
        PrintHelp();
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

    if( argc == 2 )
    {
        printf( "Usenet archive %s opened.\n", argv[1] );
        Info( *archive );
        return 0;
    }

    argc -= 2;
    argv += 2;

    if( strcmp( argv[0], "viewi" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        int idx = atoi( argv[1] );
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
    else if( strcmp( argv[0], "toplevel" ) == 0 )
    {
        auto view = archive->GetTopLevel();
        for( uint64_t i=0; i<view.size; i++ )
        {
            printf( "%i\n", view.ptr[i] );
        }
    }
    else if( strcmp( argv[0], "info" ) == 0 )
    {
        Info( *archive );
    }
    else if( strcmp( argv[0], "view" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto msg = archive->GetMessage( argv[1] );
        if( msg )
        {
            printf( "%s\n", msg );
        }
        else
        {
            printf( "Invalid message id.\n" );
        }
    }
    else if( strcmp( argv[0], "parent" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto parent = archive->GetParent( argv[1] );
        if( parent >= 0 )
        {
            printf( "Parent: %i\n", parent );
        }
        else
        {
            printf( "No parent.\n" );
        }
    }
    else if( strcmp( argv[0], "parenti" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto parent = archive->GetParent( atoi( argv[1] ) );
        if( parent >= 0 )
        {
            printf( "Parent: %i\n", parent );
        }
        else
        {
            printf( "No parent.\n" );
        }
    }
    else if( strcmp( argv[0], "child" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto children = archive->GetChildren( argv[1] );
        for( uint64_t i=0; i<children.size; i++ )
        {
            printf( "%i\n", children.ptr[i] );
        }
    }
    else if( strcmp( argv[0], "childi" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto children = archive->GetChildren( atoi( argv[1] ) );
        for( uint64_t i=0; i<children.size; i++ )
        {
            printf( "%i\n", children.ptr[i] );
        }
    }
    else if( strcmp( argv[0], "date" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto date = archive->GetDate( argv[1] );
        time_t t = { date };
        printf( "%s\n", asctime( localtime( &t ) ) );
    }
    else if( strcmp( argv[0], "datei" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto date = archive->GetDate( atoi( argv[1] ) );
        time_t t = { date };
        printf( "%s\n", asctime( localtime( &t ) ) );
    }
    else if( strcmp( argv[0], "from" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto data = archive->GetFrom( argv[1] );
        if( data )
        {
            printf( "%s\n", data );
        }
        else
        {
            printf( "Invalid message id.\n" );
        }
    }
    else if( strcmp( argv[0], "fromi" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto data = archive->GetFrom( atoi( argv[1] ) );
        if( data )
        {
            printf( "%s\n", data );
        }
        else
        {
            printf( "Invalid message id.\n" );
        }
    }
    else if( strcmp( argv[0], "subject" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto data = archive->GetSubject( argv[1] );
        if( data )
        {
            printf( "%s\n", data );
        }
        else
        {
            printf( "Invalid message id.\n" );
        }
    }
    else if( strcmp( argv[0], "subjecti" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto data = archive->GetSubject( atoi( argv[1] ) );
        if( data )
        {
            printf( "%s\n", data );
        }
        else
        {
            printf( "Invalid message id.\n" );
        }
    }
    else if( strcmp( argv[0], "idx" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        printf( "Message index: %i\n", archive->GetMessageIndex( argv[1] ) );
    }
    else if( strcmp( argv[0], "search" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        auto t0 = std::chrono::high_resolution_clock::now();
        auto results = archive->Search( argv[1], Archive::SF_AdjacentWords );
        auto& data = results.results;
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
    else if( strcmp( argv[0], "timechart" ) == 0 )
    {
        auto tc = archive->TimeChart();
        for( auto& v : tc )
        {
            printf( "%s,%i\n", v.first.c_str(), v.second );
        }
    }
    else if( strcmp( argv[0], "desc" ) == 0 )
    {
        auto d1 = archive->GetShortDescription();
        auto d2 = archive->GetLongDescription();
        auto d3 = archive->GetArchiveName();

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
        if( d3.first )
        {
            printf( "\n%.*s\n", d3.second, d3.first );
        }
        else
        {
            printf( "No archive name.\n" );
        }
    }
    else
    {
        PrintHelp();
    }

    return 0;
}
