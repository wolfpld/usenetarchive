#include <assert.h>
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
    std::sort( hvec.begin(), hvec.end(), [] ( const auto& l, const auto& r ) { return l->second * ( l->first.size() - 1 ) > r->second * ( r->first.size() - 1 ); } );

    auto trisort = new uint32_t[256*256*256];
    for( int i=0; i<256*256*256; i++ )
    {
        trisort[i] = i;
    }
    std::sort( trisort, trisort + 256*256*256, [trigram] ( const auto& l, const auto& r ) { return trigram[l] * ( l <= 256*256 ? 1 : 2 ) > trigram[r] * ( r <= 256*256 ? 1 : 2 ); } );

    auto max = std::min<int>( hvec.size(), 255 );

    uint64_t savings = 0;
    uint64_t savings2 = 0;

    printf( "#ifndef __STRING_COMPRESSION_MODEL__\n" );
    printf( "#define __STRING_COMPRESSION_MODEL__\n\n" );
    printf( "static const char* StringCompressionModel[256] = {\n" );
    printf( "\"\",\n" );
    for( int i=0; i<max; i++ )
    {
        printf( "/* 0x%02x %10i hits */ \"%s\",\n", i+1, hvec[i]->second, hvec[i]->first.c_str() );
        savings += hvec[i]->second * ( hvec[i]->first.size() - 1 );
    }
    printf( "};\n\n" );

    const char* ref[256];
    ref[0] = strdup( "" );

    printf( "static const char* TrigramDecompressionModel[256] = {\n" );
    printf( "nullptr,   // 0: end of string\n" );
    printf( "nullptr,   // 1: encoded host string\n" );
    for( int i=0; i<30; i++ )
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
        printf( "/* 0x%02x %10i hits */ \"%s\",\n", i+2, trigram[key], tmp );
        ref[i+2] = strdup( tmp );
    }
    for( int i=32; i<127; i++ )
    {
        printf( "/* 0x%02x                 */ \"%c\",\n", i, i );
        char tmp[2] = { i, 0 };
        ref[i] = strdup( tmp );
    }
    for( int i=0; i<129; i++ )
    {
        auto key = trisort[i+30];
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
        printf( "/* 0x%02x %10i hits */ \"%s\",\n", i+127, trigram[key], tmp );
        ref[i+127] = strdup( tmp );
    }
    printf( "};\n\n" );

    const char* reforig[256];
    memcpy( reforig, ref, 256 * sizeof( const char* ) );

    std::sort( ref+2, ref+256, [] ( const auto& l, const auto& r ) { return strcmp( l, r ) < 0; } );

    uint8_t rev[256];
    for( int i=2; i<256; i++ )
    {
        for( int j=2; j<256; j++ )
        {
            if( ref[i] == reforig[j] )
            {
                rev[i] = j;
                break;
            }
        }
    }

    std::vector<int> ref1, ref2, ref3;
    for( int i=2; i<256; i++ )
    {
        const auto len = strlen( ref[i] );
        switch( len )
        {
        case 1:
            ref1.emplace_back( i );
            break;
        case 2:
            ref2.emplace_back( i );
            break;
        case 3:
            ref3.emplace_back( i );
            break;
        default:
            assert( false );
            break;
        }
    }

    printf( "enum { TrigramSize1 = %i };\n", ref1.size() );
    printf( "enum { TrigramSize2 = %i };\n", ref2.size() );
    printf( "enum { TrigramSize3 = %i };\n\n", ref3.size() );

    printf( "static const char* TrigramCompressionModel1[TrigramSize1] = {\n" );
    for( auto& v : ref1 )
    {
        printf( "\"%s\",\n", ref[v] );
    }
    printf( "};\n\n" );
    printf( "static const char* TrigramCompressionIndex1[TrigramSize1] = {\n" );
    for( auto& v : ref1 )
    {
        printf( "%i,\n", rev[v] );
    }
    printf( "};\n\n" );

    printf( "static const char* TrigramCompressionModel2[TrigramSize2] = {\n");
    for( auto& v : ref2 )
    {
        printf( "\"%s\",\n", ref[v] );
    }
    printf( "};\n\n" );
    printf( "static const char* TrigramCompressionIndex2[TrigramSize2] = {\n" );
    for( auto& v : ref2 )
    {
        printf( "%i,\n", rev[v] );
    }
    printf( "};\n\n" );

    printf( "static const char* TrigramCompressionModel3[TrigramSize3] = {\n" );
    for( auto& v : ref3 )
    {
        printf( "\"%s\",\n", ref[v] );
    }
    printf( "};\n\n" );
    printf( "static const char* TrigramCompressionIndex3[TrigramSize3] = {\n" );
    for( auto& v : ref3 )
    {
        printf( "%i,\n", rev[v] );
    }
    printf( "};\n\n" );

    printf( "/* Host savings: %i KB */\n", savings / 1024 );
    printf( "/* Trigram savings: %i KB */\n\n", savings2 / 1024 );
    printf( "#endif\n" );
}
