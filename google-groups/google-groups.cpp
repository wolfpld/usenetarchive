// Based on https://github.com/henryk/gggd

#include <assert.h>
#include <curl/curl.h>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "../common/Filesystem.hpp"
#include "../common/TaskDispatch.hpp"

enum { Workers = 8 };
TaskDispatch td( Workers );

static std::mutex state;
static int pages = 0;
static int threads = 0;
static int threadstotal = 0;

static void PrintState( const char* group )
{
    printf( "\r%s .:. [P]: %i .:. [T]: %i/%i", group, pages, threads, threadstotal );
    fflush( stdout );
}

static size_t WriteFn( void* _data, size_t size, size_t num, void* ptr )
{
    const auto data = (unsigned char*)_data;
    const auto sz = size*num;
    auto& v = *(std::vector<unsigned char>*)ptr;
    v.insert( v.end(), data, data+sz );
    return sz;
}

static std::vector<unsigned char> Fetch( const std::string& url )
{
    std::vector<unsigned char> buf;

    buf.reserve( 64 * 1024 );
    auto curl = curl_easy_init();
    if( curl )
    {
        curl_easy_setopt( curl, CURLOPT_URL, url.c_str() );
        curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1L );
        curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, WriteFn );
        curl_easy_setopt( curl, CURLOPT_WRITEDATA, &buf );
        auto res = curl_easy_perform( curl );
        if( res != CURLE_OK )
        {
            buf.clear();
        }
        else
        {
            buf.emplace_back( '\0' );
        }
        curl_easy_cleanup( curl );
    }
    return buf;
}

void GetThread( const char* start, const char* end, const char* group, int len )
{

}

void GetTopics( const std::string& url, const char* group, int len )
{
    auto buf = Fetch( url );
    auto ptr = (const char*)buf.data();
    int numthr = 0;
    while( *ptr )
    {
        if( strncmp( ptr, "/topic/", 7 ) == 0 )
        {
            ptr += 8 + len;
            auto end = ptr;
            while( *end != '"' ) end++;
            td.Queue( [ptr, end, group, len] {
                GetThread( ptr, end, group, len );
            } );
            ptr = end + 1;
            numthr++;
        }
        else if( strncmp( ptr, "_escaped_fragment_", 18 ) == 0 )
        {
            auto begin = ptr - 33;
            auto end = ptr + 18;
            while( *end != '"' ) end++;
            std::string url( begin, end );
            td.Queue( [url, group, len] {
                GetTopics( url, group, len );
            } );
            ptr = end + 1;
        }
        else
        {
            ptr++;
        }
    }
    std::lock_guard<std::mutex> lock( state );
    threadstotal += numthr;
    pages++;
    PrintState( group );
}

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s group\n", argv[0] );
        exit( 1 );
    }
    if( Exists( argv[1] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }
    if( curl_global_init( CURL_GLOBAL_ALL ) != 0 )
    {
        fprintf( stderr, "Cannot initialize libcurl.\n" );
        exit( 1 );
    }

    std::string startUrl( "https://groups.google.com/forum/?_escaped_fragment_=forum/" );
    startUrl += argv[1];
    td.Queue( [startUrl, argv] {
        GetTopics( startUrl, argv[1], strlen( argv[1] ) );
    } );
    td.Sync();

    //CreateDirStruct( argv[1] );

    std::string base = argv[1];
    base.append( "/" );

    return 0;
}
