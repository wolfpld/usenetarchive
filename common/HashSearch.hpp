#ifndef __HASHSEARCH_HPP__
#define __HASHSEARCH_HPP__

#include <string>
#include <string.h>

#include "../contrib/xxhash/xxhash.h"

#include "../common/MetaView.hpp"
#include "../common/MsgIdHash.hpp"

class HashSearch
{
public:
    HashSearch( const std::string& data, const std::string& hash, const std::string& hashdata )
        : m_data( data )
        , m_hash( hash, hashdata )
        , m_mask( m_hash.Size() - 1 )
    {
    }

    HashSearch( const FileMapPtrs& data, const FileMapPtrs& hash, const FileMapPtrs& hashdata )
        : m_data( data )
        , m_hash( hash, hashdata )
        , m_mask( m_hash.Size() - 1 )
    {
    }

    int Search( const char* str ) const
    {
        const auto hash = XXH32( str, strlen( str ), 0 ) & m_mask;
        auto ptr = m_hash[hash];
        const auto num = *ptr++;
        for( uint32_t i=0; i<num; i++ )
        {
            if( strcmp( str, m_data + *ptr++ ) == 0 )
            {
                return *ptr;
            }
            ptr++;
        }
        return -1;
    }

private:
    FileMap<char> m_data;
    MetaView<uint32_t, uint32_t> m_hash;
    uint32_t m_mask;
};

#endif
