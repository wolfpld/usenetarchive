#ifndef __HASHSEARCH_HPP__
#define __HASHSEARCH_HPP__

#include <string>
#include <string.h>

#include "../contrib/xxhash/xxhash.h"

#include "../common/FileMap.hpp"
#include "../common/MsgIdHash.hpp"

template<class T>
class HashSearch
{
    struct Data
    {
        uint32_t offset;
        uint32_t idx;
    };

public:
    HashSearch( const std::string& data, const std::string& hash, const std::string& hashdata )
        : m_data( data )
        , m_hash( hash )
        , m_mask( m_hash.DataSize() - 1 )
    {
        FileMap<char> distfile( hashdata );
        m_distmax = distfile[0];
    }

    HashSearch( const FileMapPtrs& data, const FileMapPtrs& hash, const FileMapPtrs& hashdata )
        : m_data( data )
        , m_hash( hash )
        , m_mask( m_hash.DataSize() - 1 )
    {
        FileMap<char> distfile( hashdata );
        m_distmax = distfile[0];
    }

    int Search( const T* str, XXH32_hash_t _hash ) const;
    int Search( const T* str ) const;

private:
    FileMap<T> m_data;
    FileMap<Data> m_hash;
    uint32_t m_mask;
    uint8_t m_distmax;
};

template<>
int HashSearch<uint8_t>::Search( const uint8_t* str, XXH32_hash_t _hash ) const
{
    auto hash = _hash & m_mask;
    uint8_t dist = 0;
    for(;;)
    {
        auto& h = m_hash[hash];
        if( h.offset == 0 ) return -1;
        if( strcmp( (const char*)str, (const char*)(const uint8_t*)m_data + h.offset ) == 0 ) return h.idx;
        dist++;
        if( dist > m_distmax ) return -1;
        hash = (hash+1) & m_mask;
    }
}

template<>
int HashSearch<char>::Search( const char* str, XXH32_hash_t _hash ) const
{
    auto hash = _hash & m_mask;
    uint8_t dist = 0;
    for(;;)
    {
        auto& h = m_hash[hash];
        if( h.offset == 0 ) return -1;
        if( strcmp( str, m_data + h.offset ) == 0 ) return h.idx;
        dist++;
        if( dist > m_distmax ) return -1;
        hash = (hash+1) & m_mask;
    }
}

template<>
int HashSearch<uint8_t>::Search( const uint8_t* str ) const
{
    return Search( str, XXH32( str, strlen( (const char*)str ), 0 ) );
}

template<>
int HashSearch<char>::Search( const char* str ) const
{
    return Search( str, XXH32( str, strlen( str ), 0 ) );
}

#endif
