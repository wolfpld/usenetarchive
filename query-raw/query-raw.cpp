#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "../common/Filesystem.hpp"
#include "../common/HashSearch.hpp"
#include "../common/MessageView.hpp"
#include "../common/MetaView.hpp"
#include "../common/String.hpp"

#define CSTR(x) strcmp( argv[2], x ) == 0

enum class Mode
{
    Invalid,
    Info,
    List,
    QueryMsgId,
    QueryPost
};

int main( int argc, char** argv )
{
    if( argc < 3 )
    {
        fprintf( stderr, "USAGE: %s data params\n\nParams:\n", argv[0] );
        fprintf( stderr, "  -i            general information\n" );
        fprintf( stderr, "  -l            list all Message IDs\n" );
        fprintf( stderr, "  -m msgid      query Message ID\n" );
        fprintf( stderr, "  -q id         query post\n" );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }

    Mode mode = Mode::Invalid;

    if( CSTR( "-i" ) )
    {
        mode = Mode::Info;
    }
    else if( CSTR( "-l" ) )
    {
        mode = Mode::List;
    }
    else if( CSTR( "-m" ) )
    {
        if( argc < 4 )
        {
            fprintf( stderr, "Missing Message ID.\n" );
            exit( 1 );
        }
        mode = Mode::QueryMsgId;
    }
    else if( CSTR( "-q" ) )
    {
        if( argc < 4 )
        {
            fprintf( stderr, "Missing post number.\n" );
            exit( 1 );
        }
        mode = Mode::QueryPost;
    }

    if( mode == Mode::Invalid )
    {
        fprintf( stderr, "Invalid mode of operation.\n" );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const MetaView<uint32_t, char> mid( base + "midmeta", base + "middata" );
    const HashSearch hash( base + "middata", base + "midhash", base + "midhashdata" );

    const auto size = mview.Size();

    if( mode == Mode::Info )
    {
        printf( "Raw archive %s\n", argv[1] );
        printf( "  Number of posts: %i\n", size );
    }
    else if( mode == Mode::List )
    {
        for( uint32_t i=0; i<size; i++ )
        {
            const char* payload = mid[i];
            printf( "%s\n", payload );
        }
    }
    else if( mode == Mode::QueryMsgId )
    {
        auto idx = hash.Search( argv[3] );
        if( idx >= 0 )
        {
            printf( "Message idx: %i\n\n", idx );
            printf( "%s\n", mview[idx] );
            return 0;
        }
        fprintf( stderr, "Invalid Message ID %s\n", argv[3] );
        exit( 1 );
    }
    else if( mode == Mode::QueryPost )
    {
        auto idx = atoi( argv[3] );
        if( idx < 0 || idx >= size )
        {
            fprintf( stderr, "Index out of range: 0 <= %i < %i\n", idx, size );
            exit( 1 );
        }
        printf( "%s\n", mview[idx] );
    }

    return 0;
}
