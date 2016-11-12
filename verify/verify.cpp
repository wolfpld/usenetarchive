#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unordered_set>

#include "../libuat/Archive.hpp"

void RecursiveRemove( int idx, std::unordered_set<std::string>& data, const Archive& archive )
{
    data.erase( data.find( archive.GetMessageId( idx ) ) );
    const auto children = archive.GetChildren( idx );
    for( int i=0; i<children.size; i++ )
    {
        RecursiveRemove( children.ptr[i], data, archive );
    }
}

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s archive\n", argv[0] );
        exit( 1 );
    }

    auto archive = Archive::Open( argv[1] );
    if( !archive )
    {
        fprintf( stderr, "Cannot open archive %s\n", argv[1] );
        exit( 1 );
    }

    printf( "Verifying message reachibility...\n" );
    fflush( stdout );
    std::unordered_set<std::string> messages;
    const auto size = archive->NumberOfMessages();
    for( int i=0; i<size; i++ )
    {
        messages.emplace( archive->GetMessageId( i ) );
    }
    const auto top = archive->GetTopLevel();
    for( int i=0; i<top.size; i++ )
    {
        RecursiveRemove( top.ptr[i], messages, *archive );
    }
    if( messages.empty() )
    {
        printf( "All messages reachable\n" );
    }
    else
    {
        printf( "%i messages unreachable:\n", messages.size() );
        for( auto& v : messages )
        {
            printf( "%s\n", v.c_str() );
        }
    }

    return 0;
}
