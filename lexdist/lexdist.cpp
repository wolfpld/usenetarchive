#include <assert.h>
#include <algorithm>
#include <codecvt>
#include <locale>
#include <math.h>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "../common/FileMap.hpp"
#include "../common/LexiconTypes.hpp"

static int codepointlen( char c )
{
    if( ( c & 0x80 ) == 0 ) return 1;
    if( ( c & 0x20 ) == 0 ) return 2;
    if( ( c & 0x10 ) == 0 ) return 3;
    assert( ( c & 0x08 ) == 0 );
    return 4;
}

static size_t utflen( const char* str )
{
    size_t ret = 0;
    while( *str != '\0' )
    {
        str += codepointlen( *str );
        ret++;
    }
    return ret;
}

// https://en.wikibooks.org/wiki/Algorithm_Implementation/Strings/Levenshtein_distance#C.2B.2B
static int levenshtein_distance( const char* s1, const unsigned int len1, const char* s2, const unsigned int len2 )
{
    // max word length generated in lexicon is 13 letters
    static thread_local int _col[16], _prevCol[16];
    int *col = _col, *prevCol = _prevCol;

    for( unsigned int i = 0; i < len2+1; i++ )
    {
        prevCol[i] = i;
    }
    for( unsigned int i = 0; i < len1; i++ )
    {
        col[0] = i+1;
        for( unsigned int j = 0; j < len2; j++ )
        {
            col[j+1] = std::min( { prevCol[1 + j], col[j], prevCol[j] - (s1[i]==s2[j] ? 1 : 0) } ) + 1;
        }
        std::swap( col, prevCol );
    }
    return prevCol[len2];
}

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s directory\n", argv[0] );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );
    FileMap<LexiconMetaPacket> meta( base + "lexmeta" );
    FileMap<char> str( base + "lexstr" );

    const auto size = meta.DataSize();
    auto data = new std::vector<uint32_t>[size];
    auto datalock = new std::mutex[size];
    auto lengths = new unsigned int[size];
    auto stru32 = new std::u32string[size];
    auto counts = new unsigned int[size];
    auto offsets = new uint32_t[size];
    std::vector<uint32_t> byLen[LexiconMaxLen+1];

#ifdef _MSC_VER
    std::wstring_convert<std::codecvt_utf8<unsigned int>, unsigned int> conv;
#else
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> conv;
#endif
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto mp = meta + i;
        offsets[i] = mp->str;
        auto s = str + mp->str;
        auto len = utflen( s );
        assert( len <= LexiconMaxLen );

        lengths[i] = len;
#ifdef _MSC_VER
        stru32[i] = std::u32string( (const char32_t*)conv.from_bytes( s ).data() );
#else
        stru32[i] = conv.from_bytes( s );
#endif
        byLen[len].emplace_back( i );

        counts[i] = mp->dataSize;
    }

    printf( "\nWord length histogram\n" );
    for( int i=LexiconMinLen; i<=LexiconMaxLen; i++ )
    {
        printf( "%2i: %i\n", i, byLen[i].size() );
    }

    uint32_t cnt = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3F ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto mp = meta + i;
        auto s = str + mp->str;

        bool shorti = lengths[i] == 3;

        for( uint32_t j=i+1; j<size; j++ )
        {
            if( shorti && lengths[j] != 3 ) continue;
            auto dist = abs( int( lengths[i] ) - int( lengths[j] ) );
            if( dist > 2 ) continue;

            auto mp2 = meta + j;
            auto s2 = str + mp2->str;

            if( shorti )
            {
                assert( lengths[j] == 3 );
                if( levenshtein_distance( s, 3, s2, 3 ) <= 1 )
                {
                    datalock[i].lock();
                    data[i].push_back( mp2->str );
                    datalock[i].unlock();
                    datalock[j].lock();
                    data[j].push_back( mp->str );
                    datalock[j].unlock();
                }
            }
            else
            {
                if( lengths[j] > 3 && levenshtein_distance( s, lengths[i], s2, lengths[j] ) <= 2 )
                {
                    datalock[i].lock();
                    data[i].push_back( mp2->str );
                    datalock[i].unlock();
                    datalock[j].lock();
                    data[j].push_back( mp->str );
                    datalock[j].unlock();
                }
            }
        }
    }

    FILE* fdata = fopen( ( base + "lexdist" ).c_str(), "wb" );
    FILE* fmeta = fopen( ( base + "lexdistmeta" ).c_str(), "wb" );

    const uint32_t zero = 0;
    fwrite( &zero, 1, sizeof( uint32_t ), fdata );

    uint32_t offset = sizeof( uint32_t );
    for( int i=0; i<size; i++ )
    {
        if( data[i].empty() )
        {
            fwrite( &zero, 1, sizeof( uint32_t ), fmeta );
        }
        else
        {
            fwrite( &offset, 1, sizeof( uint32_t ), fmeta );
            uint32_t size = data[i].size();
            fwrite( &size, 1, sizeof( uint32_t ), fdata );
            fwrite( data[i].data(), 1, sizeof( uint32_t ) * size, fdata );
            offset += sizeof( uint32_t ) * ( size + 1 );
        }
    }

    fclose( fdata );
    fclose( fmeta );

    return 0;
}
