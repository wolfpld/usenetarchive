#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unordered_set>

#include "../libuat/Archive.hpp"

#ifdef _WIN32
#  include <windows.h>
#endif

#define MSG_OK "[\033[32;1m OK \033[0m]"
#define MSG_FAIL "[\033[31;1mFAIL\033[0m]"
#define MSG_INFO "[\033[33;1mINFO\033[0m]"

void RecursiveRemove( int idx, std::unordered_set<uint32_t>& data, const Archive& archive )
{
    auto it = data.find( idx );
    if( it == data.end() )
    {
        printf( MSG_FAIL " \033[31;1mBroken connectivity data! Aborting!\033[0m\n" );
        exit( 1 );
    }
    data.erase( it );
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
#ifdef _WIN32
    if( !AttachConsole( ATTACH_PARENT_PROCESS ) )
    {
        AllocConsole();
        SetConsoleMode( GetStdHandle( STD_OUTPUT_HANDLE ), 0x07 );
    }
#endif

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

    printf( "Verifying archive \033[37;1m%s\033[0m\n", argv[1] );

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
            printf( MSG_OK " All messages reachable\n" );
        }
        else
        {
            printf( MSG_FAIL " %i messages unreachable:\n", messages.size() );
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
            printf( MSG_OK " Messages are sorted\n" );
        }
        else
        {
            printf( MSG_FAIL " Messages are not sorted\n" );
        }
    }

    {
        auto name = archive->GetArchiveName();
        if( name.second == 0 )
        {
            printf( MSG_FAIL " Archive name not available\n" );
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
                printf( MSG_FAIL " Archive name contains newline\n" );
            }
            else
            {
                printf( MSG_OK " Archive name is valid\n" );
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
                printf( MSG_INFO " Lack of short and long descriptions\n" );
            }
            else
            {
                printf( MSG_INFO " Lack of short description\n" );
            }
        }
        else
        {
            if( dl.second == 0 )
            {
                printf( MSG_INFO " Lack of long description\n" );
            }
            else
            {
                printf( MSG_OK " Short and long descriptions are present\n" );
            }
        }
    }

    return 0;
}
