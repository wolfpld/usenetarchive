#include <chrono>
#include <memory>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <time.h>

#include "../contrib/martinus/robin_hood.h"
#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/MessageLogic.hpp"
#include "../libuat/Archive.hpp"
#include "../libuat/SearchEngine.hpp"

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
    printf( "  xref [host]   - print last message number for each host\n" );
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

    ExpandingBuffer eb;

    argc -= 2;
    argv += 2;

    if( strcmp( argv[0], "viewi" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        int idx = atoi( argv[1] );
        auto msg = archive->GetMessage( idx, eb );
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
        uint8_t pack[2048];
        archive->PackMsgId( argv[1], pack );
        auto msg = archive->GetMessage( pack, eb );
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
        uint8_t pack[2048];
        archive->PackMsgId( argv[1], pack );
        auto parent = archive->GetParent( pack );
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
        uint8_t pack[2048];
        archive->PackMsgId( argv[1], pack );
        auto children = archive->GetChildren( pack );
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
        uint8_t pack[2048];
        archive->PackMsgId( argv[1], pack );
        auto date = archive->GetDate( pack );
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
        uint8_t pack[2048];
        archive->PackMsgId( argv[1], pack );
        auto data = archive->GetFrom( pack );
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
        uint8_t pack[2048];
        archive->PackMsgId( argv[1], pack );
        auto data = archive->GetSubject( pack );
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
        uint8_t pack[2048];
        archive->PackMsgId( argv[1], pack );
        printf( "Message index: %i\n", archive->GetMessageIndex( pack ) );
    }
    else if( strcmp( argv[0], "search" ) == 0 )
    {
        if( argc == 1 ) BadArg();
        SearchEngine search( *archive );
        auto t0 = std::chrono::high_resolution_clock::now();
        auto results = search.Search( argv[1], SearchEngine::SF_AdjacentWords );
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
    else if( strcmp( argv[0], "xref" ) == 0 )
    {
        const auto desc = archive->GetArchiveName();
        if( !desc.first )
        {
            fprintf( stderr, "Archive doesn't have embedded name.\n" );
            exit( 1 );
        }

        const char* host = argc == 1 ? nullptr : argv[1];
        const auto hlen = host ? strlen( host ) : 0;

        const auto num = archive->NumberOfMessages();
        robin_hood::unordered_flat_map<std::string, int> latest;
        for( int i=0; i<num; i++ )
        {
            auto post = archive->GetMessage( i, eb );
            auto ptr = FindOptionalHeader( post, "xref: ", 6 );
            if( *ptr == '\n' ) continue;
            ptr += 6;
            auto end = ptr;
            while( *end != ' ' ) end++;
            if( host && ( end - ptr != hlen || strncmp( ptr, host, hlen ) != 0 ) ) continue;
            std::string server( ptr, end );

            while( *end != '\n' )
            {
                ptr = end = end + 1;
                while( *end != ':' ) end++;
                if( end - ptr == desc.second && strncmp( ptr, desc.first, desc.second ) == 0 )
                {
                    end++;
                    const auto n = atoi( end );
                    auto it = latest.find( server );
                    if( it == latest.end() )
                    {
                        latest.emplace( std::move( server ), n );
                    }
                    else
                    {
                        if( it->second < n )
                        {
                            it->second = n;
                        }
                    }
                    break;
                }
                else
                {
                    while( *end != '\n' && *end != ' ' ) end++;
                }
            }
        }

        if( host )
        {
            auto it = latest.find( host );
            if( it == latest.end() )
            {
                printf( "Not found.\n" );
            }
            else
            {
                printf( "%i\n", it->second );
            }
        }
        else
        {
            for( auto& v : latest )
            {
                printf( "%s %i\n", v.first.c_str(), v.second );
            }
        }
    }
    else
    {
        PrintHelp();
    }

    return 0;
}
