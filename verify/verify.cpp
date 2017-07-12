#include <functional>
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

enum class State
{
    Ok,
    Fail,
    Info
};

bool quiet = false;
const char* path = nullptr;

void PrintInfo( State state, const char* message )
{
    if( quiet )
    {
        if( state == State::Fail )
        {
            printf( MSG_FAIL " \033[37;1m%s\033[0m: %s\n", path, message );
        }
    }
    else
    {
        switch( state )
        {
        case State::Ok:
            printf( MSG_OK " " );
            break;
        case State::Fail:
            printf( MSG_FAIL " " );
            break;
        case State::Info:
            printf( MSG_INFO " " );
            break;
        }
        printf( "%s\n", message );
    }
}

void RecursiveRemove( int idx, std::unordered_set<uint32_t>& data, const Archive& archive )
{
    auto it = data.find( idx );
    if( it == data.end() )
    {
        PrintInfo( State::Fail, "\033[31;1mBroken connectivity data! Aborting!\033[0m" );
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

void Usage( const char* image )
{
    fprintf( stderr, "USAGE: %s [-q] archive\n", image );
    exit( 1 );
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

    if( argc < 2 ) Usage( argv[0] );

    path = argv[1];
    if( strcmp( path, "-q" ) == 0 )
    {
        if( argc == 2 ) Usage( argv[0] );
        path = argv[2];
        quiet = true;
    }

    auto archive = Archive::Open( path );
    if( !archive )
    {
        fprintf( stderr, "Cannot open archive %s\n", path );
        exit( 1 );
    }

    if( !quiet )
    {
        printf( "Verifying archive \033[37;1m%s\033[0m\n", path );
    }

    const auto size = archive->NumberOfMessages();

    // message reachibility
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
            PrintInfo( State::Ok, "All messages reachable" );
        }
        else
        {
            PrintInfo( State::Fail, "Some messages are unreachable" );
        }
    }

    // message sorting
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
            PrintInfo( State::Ok, "Messages are sorted" );
        }
        else
        {
            PrintInfo( State::Fail, "Messages are not sorted" );
        }
    }

    // archive name
    {
        auto name = archive->GetArchiveName();
        if( name.second == 0 )
        {
            PrintInfo( State::Fail, "Archive name not available" );
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
                PrintInfo( State::Fail, "Archive name contains newline" );
            }
            else
            {
                PrintInfo( State::Ok, "Archive name is valid" );
            }
        }
    }

    // archive description
    {
        auto ds = archive->GetShortDescription();
        auto dl = archive->GetLongDescription();
        if( ds.second == 0 )
        {
            if( dl.second == 0 )
            {
                PrintInfo( State::Info, "Lack of short and long descriptions" );
            }
            else
            {
                PrintInfo( State::Info, "Lack of short description" );
            }
        }
        else
        {
            if( dl.second == 0 )
            {
                PrintInfo( State::Info, "Lack of long description" );
            }
            else
            {
                PrintInfo( State::Ok, "Short and long descriptions are present" );
            }
        }
    }

    // children total counts
    {
        std::vector<int> v( size, -1 );

        std::function<int(int)> Count;
        Count = [&v, &archive, &Count]( int idx ) {
            int cnt = 1;
            auto children = archive->GetChildren( idx );
            for( int j=0; j<children.size; j++ )
            {
                if( v[children.ptr[j]] == -1 )
                {
                    cnt += Count( children.ptr[j] );
                }
                else
                {
                    cnt += v[children.ptr[j]];
                }
            }
            return cnt;
        };

        bool ok = true;
        for( int i=0; i<size; i++ )
        {
            if( Count( i ) != archive->GetTotalChildrenCount( i ) )
            {
                ok = false;
                break;
            }
        }

        if( ok )
        {
            PrintInfo( State::Ok, "Total children counts valid" );
        }
        else
        {
            PrintInfo( State::Fail, "Wrong total children counts" );
        }
    }

    // duplicates
    {
        std::unordered_set<std::string> unique;
        bool ok = true;
        for( int i=0; i<size; i++ )
        {
            std::string msgid = archive->GetMessageId( i );
            if( unique.find( msgid ) == unique.end() )
            {
                unique.emplace( std::move( msgid ) );
            }
            else
            {
                ok = false;
                break;
            }
        }

        if( ok )
        {
            PrintInfo( State::Ok, "No duplicate messages found" );
        }
        else
        {
            PrintInfo( State::Fail, "Message duplicates exist" );
        }
    }

    delete archive;

    return 0;
}
