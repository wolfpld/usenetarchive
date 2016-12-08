#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_set>

#include "../libuat/Archive.hpp"

void RecursiveRemove( int idx, std::unordered_set<uint32_t>& data, const Archive& archive )
{
    data.erase( data.find( idx ) );
    const auto children = archive.GetChildren( idx );
    for( int i=0; i<children.size; i++ )
    {
        RecursiveRemove( children.ptr[i], data, archive );
    }
}

void Expand( std::vector<uint32_t>& order, Archive& archive, uint32_t msgidx )
{
    auto children = archive.GetChildren( msgidx );
    for( int i=0; i<children.size; i++ )
    {
        order.emplace_back( children.ptr[i] );
        Expand( order, archive, order.back() );
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

    const auto size = archive->NumberOfMessages();

    {
        std::unordered_set<uint32_t> messages;
        messages.reserve( size );
        for( int i=0; i<size; i++ )
        {
            messages.emplace( i );
        }
        const auto top = archive->GetTopLevel();
        for( int i=0; i<top.size; i++ )
        {
            RecursiveRemove( top.ptr[i], messages, *archive );
        }
        if( messages.empty() )
        {
            printf( "[ OK ] All messages reachable\n" );
        }
        else
        {
            printf( "[FAIL] %i messages unreachable:\n", messages.size() );
            for( auto& v : messages )
            {
                printf( "      -> %s\n", archive->GetMessageId( v ) );
            }
        }
    }

    {
        std::vector<uint32_t> order;
        order.reserve( size );
        auto toplevel = archive->GetTopLevel();
        for( int i=0; i<toplevel.size; i++ )
        {
            order.push_back( toplevel.ptr[i] );
            Expand( order, *archive, order.back() );
        }
        assert( order.size() == size );

        bool ok = true;
        for( int i=0; i<size; i++ )
        {
            if( order[i] != i )
            {
                ok = false;
                break;
            }
        }
        if( ok )
        {
            printf( "[ OK ] Messages are sorted\n" );
        }
        else
        {
            printf( "[FAIL] Messages are not sorted\n" );
        }
    }

    {
        auto name = archive->GetArchiveName();
        if( name.second == 0 )
        {
            printf( "[FAIL] Archive name not available\n" );
        }
        else
        {
            bool bad = false;
            for( int i=0; i<name.second; i++ )
            {
                if( name.first[i] == '\n' )
                {
                    bad = true;
                    break;
                }
            }
            if( bad )
            {
                printf( "[FAIL] Archive name contains newline\n" );
            }
            else
            {
                printf( "[ OK ] Archive name is valid\n" );
            }
        }
    }

    {
        auto ds = archive->GetShortDescription();
        auto dl = archive->GetLongDescription();
        if( ds.second == 0 )
        {
            if( dl.second == 0 )
            {
                printf( "[INFO] Lack of short and long descriptions\n" );
            }
            else
            {
                printf( "[INFO] Lack of short description\n" );
            }
        }
        else
        {
            if( dl.second == 0 )
            {
                printf( "[INFO] Lack of long description\n" );
            }
            else
            {
                printf( "[ OK ] Short and long descriptions are present\n" );
            }
        }
    }

    return 0;
}
