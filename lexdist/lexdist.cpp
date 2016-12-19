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
#include "../common/System.hpp"
#include "../common/TaskDispatch.hpp"

static int CountBits( uint64_t i )
{
    i = i - ( (i >> 1) & 0x5555555555555555 );
    i = ( i & 0x3333333333333333 ) + ( (i >> 2) & 0x3333333333333333 );
    i = ( (i + (i >> 4) ) & 0x0F0F0F0F0F0F0F0F );
    return ( i * (0x0101010101010101) ) >> 56;
}

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
static int levenshtein_distance( const char32_t* s1, const unsigned int len1, const char32_t* s2, const unsigned int len2, int threshold )
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
        auto min = col[0];
        for( unsigned int j = 0; j < len2; j++ )
        {
            col[j+1] = std::min( { prevCol[1 + j], col[j], prevCol[j] - (s1[i]==s2[j] ? 1 : 0) } ) + 1;
            if( col[j+1] < min ) min = col[j+1];
        }
        if( min >= threshold ) return threshold;
        std::swap( col, prevCol );
    }
    return prevCol[len2];
}

static int GetMaxLD( int len )
{
    if( len <= 5 ) return 1;
    if( len <= 10 ) return 2;
    return 3;
}

void BuildHeuristicData( uint64_t& v, const char* str )
{
    v = 0;
    while( *str != '\0' )
    {
        auto cpl = codepointlen( *str );
        if( cpl > 1 )
        {
            str += cpl;
            v |= 0x1;
        }
        else
        {
            if( *str >= '0' && *str <= '9' )
            {
                v |= ( 1LL << ( *str - '0' + 1 ) );
            }
            else if( *str >= 'a' && *str <= 'z' )
            {
                v |= ( 1LL << ( *str - 'a' + 12 ) );
            }
            else if( *str == '.' )
            {
                v |= ( 1LL << 39 );
            }
            else
            {
                v |= 0x1;
            }
            str++;
        }
    }
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
    auto lengths = new unsigned int[size];
    auto stru32 = new std::u32string[size];
    auto counts = new unsigned int[size];
    auto offsets = new uint32_t[size];
    auto heurdata = new uint64_t[size];
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

        BuildHeuristicData( heurdata[i], s );
    }

    printf( "\nWord length histogram\n" );
    for( int i=LexiconMinLen; i<=LexiconMaxLen; i++ )
    {
        printf( "%2i: %i\n", i, byLen[i].size() );
    }

    const auto cpus = System::CPUCores();
    printf( "Working... (%i threads)\n", cpus );

    std::mutex mtx;
    TaskDispatch tasks( cpus );
    const auto taskNum = cpus * 16;

    for( int i=LexiconMinLen; i<=LexiconMaxLen; i++ )
    {
        const auto maxld = GetMaxLD( i );
        const auto ldstart = std::max<int>( i-maxld, LexiconMinLen );
        const auto ldend = std::min<int>( i+maxld, LexiconMaxLen );
        const auto& byLen1 = byLen[i];

        const auto size = byLen1.size();
        uint32_t start = 0;
        uint32_t inPass = ( size + taskNum - 1 ) / taskNum;
        uint32_t left = size;
        uint32_t cnt = 0;
        for( int t=0; t<taskNum; t++ )
        {
            uint32_t todo = std::min( left, inPass );
            tasks.Queue( [&stru32, &byLen1, &byLen, start, todo, size, &cnt, &mtx, i, counts, ldstart, ldend, maxld, offsets, &data, heurdata]() {
                std::vector<std::pair<unsigned int, uint32_t>> candidates;
                for( int j=start; j<start+todo; j++ )
                {
                    mtx.lock();
                    auto c = cnt++;
                    mtx.unlock();
                    if( ( c & 0x1FF ) == 0 )
                    {
                        printf( "%2i: %i/%i\r", i, c, size );
                        fflush( stdout );
                    }

                    const auto idx = byLen1[j];
                    const auto cnt = counts[idx];
                    const auto tcnt = cnt / 10;    // 10%
                    const auto& str1 = stru32[idx];
                    const auto heur1 = heurdata[idx];

                    unsigned int maxCount = 0;
                    candidates.clear();
                    for( int k=ldstart; k<=ldend; k++ )
                    {
                        const auto& byLen2 = byLen[k];
                        const auto size2 = byLen2.size();
                        for( int l=0; l<size2; l++ )
                        {
                            const auto idx2 = byLen2[l];
                            const auto cnt2 = counts[idx2];
                            if( cnt2 >= tcnt )
                            {
                                const auto heur2 = heurdata[idx2];
                                if( CountBits( heur1 ^ heur2 ) <= maxld * 2 )
                                {
                                    const auto& str2 = stru32[idx2];
                                    const auto ld = levenshtein_distance( str1.c_str(), i, str2.c_str(), k, maxld+1 );
                                    if( ld > 0 && ld <= maxld )
                                    {
                                        candidates.emplace_back( cnt2, offsets[idx2] );
                                        if( cnt2 > maxCount ) maxCount = cnt2;
                                    }
                                }
                            }
                        }
                    }
                    const auto tmc = maxCount / 5;  // 20%
                    for( auto& v : candidates )
                    {
                        if( v.first >= tmc )
                        {
                            data[idx].emplace_back( v.second );
                        }
                    }
                }
            } );
            start += todo;
            left -= todo;
        }
        tasks.Sync();
        printf( "%2i: %i/%i\n", i, size, size );
    }

    FILE* fdata = fopen( ( base + "lexdist" ).c_str(), "wb" );
    FILE* fmeta = fopen( ( base + "lexdistmeta" ).c_str(), "wb" );

    const uint32_t zero = 0;
    fwrite( &zero, 1, sizeof( uint32_t ), fdata );

    int statnone = 0;
    int statcnt = 0;
    int statnum = 0;

    uint32_t offset = sizeof( uint32_t );
    for( int i=0; i<size; i++ )
    {
        if( data[i].empty() )
        {
            fwrite( &zero, 1, sizeof( uint32_t ), fmeta );
            statnone++;
        }
        else
        {
            fwrite( &offset, 1, sizeof( uint32_t ), fmeta );
            uint32_t size = data[i].size();
            fwrite( &size, 1, sizeof( uint32_t ), fdata );
            fwrite( data[i].data(), 1, sizeof( uint32_t ) * size, fdata );
            offset += sizeof( uint32_t ) * ( size + 1 );
            statnum++;
            statcnt += size;
        }
    }

    fclose( fdata );
    fclose( fmeta );

    if( statnum == 0 )
    {
        printf( "No similar words found for %i words.\n", statnone );
    }
    else
    {
        printf( "No similar words found for %i words. Average %.2f similar words found for %i words.\n", statnone, float( statcnt ) / statnum, statnum );
    }

    return 0;
}
