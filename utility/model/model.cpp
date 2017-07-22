#include <algorithm>
#include <map>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "../../common/FileMap.hpp"

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "Usage: model input\n" );
        exit( 1 );
    }

    std::map<std::string, uint32_t> hosts;
    auto trigram = new uint64_t[256*256*256];

    FileMap<char> in( argv[1] );
    const auto size = in.DataSize();
    const char* ptr = in;

    while( ptr - in < size )
    {
        auto end = ptr;
        while( *end != '\0' ) end++;

        auto host = ptr;
        while( host < end && *host != '@' ) host++;
        if( host != end )
        {
            hosts[std::string( host, end )]++;
        }

        const auto len = host - ptr;
        for( int i=0; i<len-1; i++ )
        {
            trigram[ptr[i]*256 + ptr[i+1]]++;
        }
        for( int i=0; i<len-2; i++ )
        {
            trigram[ptr[i]*256*256 + ptr[i+1]*256 + ptr[i+2]]++;
        }

        ptr = end+1;
    }

    std::vector<std::map<std::string, uint32_t>::iterator> hvec;
    hvec.reserve( hosts.size() );
    for( auto it = hosts.begin(); it != hosts.end(); ++it )
    {
        hvec.emplace_back( it );
    }
    std::sort( hvec.begin(), hvec.end(), [] ( const auto& l, const auto& r ) { return l->second * l->first.size() > r->second * r->first.size(); } );

    auto trisort = new uint32_t[256*256*256];
    for( int i=0; i<256*256*256; i++ )
    {
        trisort[i] = i;
    }
    std::sort( trisort, trisort + 256*256*256, [trigram] ( const auto& l, const auto& r ) { return trigram[l] * ( l <= 256*256 ? 2 : 3 ) > trigram[r] * ( r <= 256*256 ? 2 : 3 ); } );

    auto max = std::min<int>( hvec.size(), 255 );

    uint64_t savings = 0;
    uint64_t savings2 = 0;

    printf( "#ifndef __STRING_COMPRESSION_MODEL__\n" );
    printf( "#define __STRING_COMPRESSION_MODEL__\n\n" );
    printf( "static const char* StringCompressionModel[256] = {\n" );
    printf( "\"\\0\",\n" );
    for( int i=0; i<max; i++ )
    {
        printf( "/* 0x%02x %10i hits */ \"%s\",\n", i+1, hvec[i]->second, hvec[i]->first.c_str() );
        savings += hvec[i]->second * ( hvec[i]->first.size() - 1 );
    }
    printf( "}\n\n" );

    printf( "static const char* TrigramCompressionModel[256] = {\n" );
    printf( "\"\\0\",\n" );
    for( int i=0; i<31; i++ )
    {
        auto key = trisort[i];
        char tmp[4] = {};
        if( key <= 256*256 )
        {
            tmp[0] = ( key & 0xFF00 ) >> 8;
            tmp[1] = key & 0xFF;
            savings2 += trigram[key];
        }
        else
        {
            tmp[0] = ( key & 0xFF0000 ) >> 16;
            tmp[1] = ( key & 0xFF00 ) >> 8;
            tmp[2] = key & 0xFF;
            savings2 += trigram[key] * 2;
        }
        printf( "/* 0x%02x %10i hits */ \"%s\",\n", i+1, trigram[key], tmp );
    }
    for( int i=32; i<127; i++ )
    {
        printf( "/* 0x%02x                 */ \"%c\",\n", i, i );
    }
    for( int i=0; i<128; i++ )
    {
        auto key = trisort[i+31];
        char tmp[4] = {};
        if( key <= 256*256 )
        {
            tmp[0] = ( key & 0xFF00 ) >> 8;
            tmp[1] = key & 0xFF;
            savings2 += trigram[key];
        }
        else
        {
            tmp[0] = ( key & 0xFF0000 ) >> 16;
            tmp[1] = ( key & 0xFF00 ) >> 8;
            tmp[2] = key & 0xFF;
            savings2 += trigram[key] * 2;
        }
        printf( "/* 0x%02x %10i hits */ \"%s\",\n", i+128, trigram[key], tmp );
    }
    printf( "}\n\n" );

    printf( "/* Host savings: %i KB */\n", savings / 1024 );
    printf( "/* Trigram savings: %i KB */\n\n", savings2 / 1024 );
    printf( "#endif\n" );
}
