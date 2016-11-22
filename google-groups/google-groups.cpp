// Based on https://github.com/henryk/gggd

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <curl/curl.h>

#include "../common/Filesystem.hpp"

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
        curl_easy_cleanup( curl );
    }
    return buf;
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



    //CreateDirStruct( argv[1] );

    std::string base = argv[1];
    base.append( "/" );

    return 0;
}
