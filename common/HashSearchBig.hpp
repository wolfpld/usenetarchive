#ifndef __HASHSEARCHBIG_HPP__
#define __HASHSEARCHBIG_HPP__

#include <string>
#include <string.h>

#include "../contrib/xxhash/xxhash.h"

#include "../common/MetaView.hpp"
#include "../common/MsgIdHash.hpp"

class HashSearchBig
{
public:
    HashSearchBig( const std::string& msgidmeta, const std::string& msgid, const std::string& hashmeta, const std::string& hashdata )
        : m_data( msgidmeta, msgid )
        , m_hash( hashmeta, hashdata )
        , m_mask( m_hash.Size() - 1 )
    {
    }

    int Search( const char* str, XXH32_hash_t _hash ) const
    {
        const auto hash = _hash & m_mask;
        auto ptr = m_hash[hash];
        const auto num = *ptr++;
        for( uint32_t i=0; i<num; i++ )
        {
            if( strcmp( str, m_data[*ptr] ) == 0 )
            {
                return *ptr;
            }
            ptr++;
        }
        return -1;
    }

    int Search( const char* str ) const
    {
        return Search( str, XXH32( str, strlen( str ), 0 ) );
    }

private:
    MetaView<uint64_t, char> m_data;
    MetaView<uint64_t, uint32_t> m_hash;
    uint32_t m_mask;
};

#endif
