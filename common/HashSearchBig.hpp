#ifndef __HASHSEARCHBIG_HPP__
#define __HASHSEARCHBIG_HPP__

#include <stdint.h>
#include <string>
#include <string.h>

#include "../contrib/xxhash/xxhash.h"

#include "../common/FileMap.hpp"
#include "../common/MetaView.hpp"
#include "../common/MsgIdHash.hpp"

class HashSearchBig
{
    struct Data
    {
        uint64_t offset;
        uint64_t idx;
    };

public:
    HashSearchBig( const std::string& msgid, const std::string& hashmeta, const std::string& hashdata )
        : m_data( msgid )
        , m_hash( hashdata )
        , m_mask( m_hash.DataSize() - 1 )
    {
        FileMap<char> distfile( hashmeta );
        m_distmax = distfile[0];
    }

    int Search( const char* str, XXH32_hash_t _hash ) const
    {
        auto hash = _hash & m_mask;
        uint8_t dist = 0;
        for(;;)
        {
            if( m_hash[hash].offset == 0 ) return -1;
            if( strcmp( str, m_data + m_hash[hash].offset ) == 0 ) return m_hash[hash].idx;
            dist++;
            if( dist > m_distmax ) return -1;
            hash = (hash+1) & m_mask;
        }
    }

    int Search( const char* str ) const
    {
        return Search( str, XXH32( str, strlen( str ), 0 ) );
    }

private:
    FileMap<char> m_data;
    FileMap<Data> m_hash;
    uint32_t m_mask;
    uint8_t m_distmax;
};

#endif
