#include <algorithm>
#include <assert.h>
#include <limits>
#include <unordered_map>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "../common/Filesystem.hpp"
#include "../common/MessageView.hpp"
#include "../common/String.hpp"

#include "tin.hpp"

struct Offsets
{
    uint32_t from;
    uint32_t subject;
    uint32_t realname;
};

struct OptEntry
{
    uint32_t idx;
    uint32_t offset;
};

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s raw\n", argv[0] );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const auto size = mview.Size();

    std::vector<std::string> strings;
    Offsets* data = new Offsets[size];

    std::unordered_map<std::string, uint32_t> refs;

    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto buf = post;

        bool fdone = false;
        bool sdone = false;
        const char* fptr;
        const char* sptr;
        while( !fdone || !sdone )
        {
            bool skip = false;
            if( !fdone )
            {
                if( strnicmpl( buf, "from: ", 6 ) == 0 )
                {
                    buf += 6;
                    fptr = buf;
                    skip = true;
                    fdone = true;
                }
            }
            if( !sdone && !skip )
            {
                if( strnicmpl( buf, "subject: ", 9 ) == 0 )
                {
                    buf += 9;
                    sptr = buf;
                    sdone = true;
                }
            }
            while( *buf++ != '\n' ) {};
        }

        auto fend = fptr;
        while( *++fend != '\n' ) {};
        auto send = sptr;
        while( *++send != '\n' ) {};

        std::string fstr( fptr, fend );
        std::string sstr( sptr, send );
        std::string rstr = GetUserName( fstr.c_str() );

        if( refs.find( fstr ) == refs.end() )
        {
            refs.emplace( fstr, strings.size() );
            strings.emplace_back( fstr );
        }
        if( refs.find( sstr ) == refs.end() )
        {
            refs.emplace( sstr, strings.size() );
            strings.emplace_back( sstr );
        }
        if( refs.find( rstr ) == refs.end() )
        {
            refs.emplace( rstr, strings.size() );
            strings.emplace_back( rstr );
        }

        data[i].from = refs[fstr];
        data[i].subject = refs[sstr];
        data[i].realname = refs[rstr];
    }

    printf( "\nOptimizing...\n" );
    fflush( stdout );

    std::vector<size_t> lengths;
    lengths.reserve( strings.size() );
    for( auto& v : strings )
    {
        lengths.emplace_back( v.size() );
    }

    std::vector<size_t> order;
    order.reserve( strings.size() );
    for( int i=0; i<strings.size(); i++ )
    {
        order.emplace_back( i );
    }

    std::sort( order.begin(), order.end(), [&lengths]( const auto& l, const auto& r ) { return lengths[l] > lengths[r]; } );

    uint32_t limit = 0;
    uint32_t prevLen = std::numeric_limits<uint32_t>::max();

    std::vector<std::string> outStrings;
    std::vector<OptEntry> outData( strings.size() );

    outStrings.reserve( strings.size() );

    unsigned int savings = 0;
    for( int i=0; i<strings.size(); i++ )
    {
        if( ( i & 0x1FF ) == 0 )
        {
            printf( "%i/%i\r", i, strings.size() );
            fflush( stdout );
        }

        auto idx = order[i];
        if( lengths[idx] < prevLen )
        {
            limit = outStrings.size();
            prevLen = lengths[idx];
        }

        bool done = false;
        for( uint32_t j=0; j<limit; j++ )
        {
            uint32_t offset = outStrings[j].size() - lengths[idx];
            if( memcmp( outStrings[j].c_str() + offset, strings[idx].c_str(), lengths[idx] ) == 0 )
            {
                savings += lengths[idx];
                outData[idx] = OptEntry{ j, offset };
                done = true;
                break;
            }
        }
        if( !done )
        {
            outData[idx] = OptEntry{ uint32_t( outStrings.size() ), 0 };
            outStrings.emplace_back( strings[idx] );
        }
    }

    printf( "\nOptimization savings: %iKB\n", savings / 1024 );
    printf( "Saving...\n" );
    fflush( stdout );

    std::vector<uint32_t> strOffsets;
    strOffsets.reserve( outStrings.size() );

    FILE* strout = fopen( ( base + "strings" ).c_str(), "wb" );
    uint32_t offset = 0;
    for( auto& v : outStrings )
    {
        strOffsets.emplace_back( offset );
        offset += fwrite( v.c_str(), 1, v.size()+1, strout );
    }
    fclose( strout );

    printf( "Strings DB size: %iKB\n", offset / 1024 );
    fflush( stdout );

    FILE* out = fopen( ( base + "strmeta" ).c_str(), "wb" );
    for( uint32_t i=0; i<size; i++ )
    {
        uint32_t fo = strOffsets[outData[data[i].from].idx] + outData[data[i].from].offset;
        uint32_t so = strOffsets[outData[data[i].subject].idx] + outData[data[i].subject].offset;
        uint32_t ro = strOffsets[outData[data[i].realname].idx] + outData[data[i].realname].offset;

        fwrite( &fo, 1, sizeof( fo ), out );
        fwrite( &so, 1, sizeof( so ), out );
        fwrite( &ro, 1, sizeof( ro ), out );
    }
    fclose( out );

    delete[] data;

    return 0;
}
