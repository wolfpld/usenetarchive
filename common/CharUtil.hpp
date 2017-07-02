#ifndef __DARKRL__CHARUTIL_HPP__
#define __DARKRL__CHARUTIL_HPP__

#include <stddef.h>
#include <string.h>

#include "../contrib/xxhash/xxhash.h"

namespace CharUtil
{

    struct Hasher
    {
        size_t operator()( const char* key ) const
        {
            return XXH32( key, strlen( key ), 0 );
        }
    };

    struct Comparator
    {
        bool operator()( const char* lhs, const char* rhs ) const
        {
            return strcmp( lhs, rhs ) == 0;
        }
    };

    struct LessComparator
    {
        bool operator()( const char* lhs, const char* rhs ) const
        {
            return strcmp( lhs, rhs ) < 0;
        }
    };

}

#endif
