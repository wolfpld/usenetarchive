#include <algorithm>
#include <assert.h>

#include "StringCompress.hpp"

size_t StringCompress::Pack( const char* in, uint8_t* out )
{
    const uint8_t* refout = out;

    while( *in != 0 )
    {
        auto it3 = std::lower_bound( TrigramCompressionModel3, TrigramCompressionModel3 + TrigramSize3, in, [] ( const auto& l, const auto& r ) { return strncmp( l, r, 3 ) < 0; } );
        if( it3 != TrigramCompressionModel3 + TrigramSize3 && strncmp( *it3, in, 3 ) == 0 )
        {
            *out++ = TrigramCompressionIndex3[it3 - TrigramCompressionModel3];
            in += 3;
            continue;
        }
        auto it2 = std::lower_bound( TrigramCompressionModel2, TrigramCompressionModel2 + TrigramSize2, in, [] ( const auto& l, const auto& r ) { return strncmp( l, r, 2 ) < 0; } );
        if( it2 != TrigramCompressionModel2 + TrigramSize2 && strncmp( *it2, in, 2 ) == 0 )
        {
            *out++ = TrigramCompressionIndex2[it2 - TrigramCompressionModel2];
            in += 2;
            continue;
        }
        assert( *in >= 32 && *in <= 126 );
        *out++ = *in++;
    }

    *out++ = 0;

    return out - refout;
}

size_t StringCompress::Unpack( const uint8_t* in, char* out )
{
    const char* refout = out;

    while( *in != 0 )
    {
        const char* dec = TrigramDecompressionModel[*in++];
        while( *dec != '\0' ) *out++ = *dec++;
    }

    *out++ = '\0';

    return out - refout;
}
