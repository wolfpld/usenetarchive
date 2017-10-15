#include <algorithm>
#include <assert.h>
#include <limits>
#include <unordered_map>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unordered_set>
#include <vector>

#include "../common/CharUtil.hpp"
#include "../common/Filesystem.hpp"
#include "../common/MessageView.hpp"
#include "../common/String.hpp"

#include "tin.hpp"

static const char* EmptySubject = "(no subject)\n";

struct Offsets
{
    uint32_t from;
    uint32_t subject;
    uint32_t realname;
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
                    sptr = *buf != '\n' ? buf : EmptySubject;
                    sdone = true;
                }
            }
            while( *buf++ != '\n' ) {};
            if( *buf == '\n' ) break;
        }

        if( !sdone )
        {
            sptr = EmptySubject;
        }
        assert( fdone );

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

    const int bufSize = 1024*1024*64;
    char* buf = new char[bufSize];

    std::unordered_set<const char*, CharUtil::Hasher, CharUtil::Comparator> avail;
    std::vector<uint32_t> outOffset( strings.size() );

    unsigned int savings = 0;
    uint32_t offset = 0;
    for( int i=0; i<strings.size(); i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, strings.size() );
            fflush( stdout );
        }

        auto idx = order[i];
        auto strptr = strings[idx].c_str();
        auto it = avail.find( strptr );
        if( it == avail.end() )
        {
            outOffset[idx] = offset;
            auto ss = strings[idx].size() + 1;
            assert( offset + ss <= bufSize );
            memcpy( buf + offset, strptr, ss );
            for( int j=1; j<ss-1; j++ )
            {
                if( avail.find( buf + offset + j ) == avail.end() )
                {
                    avail.emplace( buf + offset + j );
                }
                else
                {
                    break;
                }
            }
            offset += ss;
        }
        else
        {
            savings += lengths[idx];
            outOffset[idx] = *it - buf;
        }
    }

    printf( "\nOptimization savings: %iKB\n", savings / 1024 );
    printf( "Saving...\n" );
    fflush( stdout );

    FILE* strout = fopen( ( base + "strings" ).c_str(), "wb" );
    fwrite( buf, 1, offset, strout );
    fclose( strout );

    printf( "Strings DB size: %iKB\n", offset / 1024 );
    fflush( stdout );

    FILE* out = fopen( ( base + "strmeta" ).c_str(), "wb" );
    for( uint32_t i=0; i<size; i++ )
    {
        uint32_t fo = outOffset[data[i].from];
        uint32_t so = outOffset[data[i].subject];
        uint32_t ro = outOffset[data[i].realname];

        fwrite( &fo, 1, sizeof( fo ), out );
        fwrite( &so, 1, sizeof( so ), out );
        fwrite( &ro, 1, sizeof( ro ), out );
    }
    fclose( out );

    delete[] data;
    delete[] buf;

    return 0;
}
