#ifndef __STRING_COMPRESSION_MODEL__
#define __STRING_COMPRESSION_MODEL__

#include <algorithm>
#include <set>
#include <stdint.h>
#include <string>
#include <string.h>
#include <unordered_map>
#include <vector>

#include "CharUtil.hpp"
#include "FileMap.hpp"

class StringCompress
{
public:
    template<class T>
    StringCompress( const T& strings );
    StringCompress( const std::string& fn );
    StringCompress( const FileMapPtrs& ptrs );
    ~StringCompress();

    size_t Pack( const char* in, uint8_t* out ) const;
    size_t Unpack( const uint8_t* in, char* out ) const;
    size_t Repack( const uint8_t* in, uint8_t* out, const StringCompress& other ) const;

    void WriteData( const std::string& fn ) const;

private:
    StringCompress( const StringCompress& ) = delete;
    StringCompress( StringCompress&& ) = delete;

    StringCompress& operator=( const StringCompress& ) = delete;
    StringCompress& operator=( StringCompress&& ) = delete;

    const char* m_data;
    uint32_t m_dataLen;
    uint8_t m_maxHost;
    uint8_t m_hostLookup[255];
    uint32_t m_hostOffset[255];
};


template<class T>
StringCompress::StringCompress( const T& strings )
{
    std::unordered_map<const char*, uint64_t, CharUtil::Hasher, CharUtil::Comparator> hosts;

    for( auto v : strings )
    {
        while( *v != '@' && *v != '\0' ) v++;
        if( *v == '@' )
        {
            v++;
            auto it = hosts.find( v );
            if( it == hosts.end() )
            {
                hosts.emplace( v, 1 );
            }
            else
            {
                it->second++;
            }
        }
    }

    std::vector<std::unordered_map<const char*, uint64_t, CharUtil::Hasher, CharUtil::Comparator>::iterator> hvec;
    hvec.reserve( hosts.size() );
    for( auto it = hosts.begin(); it != hosts.end(); ++it )
    {
        hvec.emplace_back( it );
    }
    std::sort( hvec.begin(), hvec.end(), [] ( const auto& l, const auto& r ) { return l->second * ( strlen( l->first ) - 1 ) > r->second * ( strlen( r->first ) - 1 ); } );

    m_dataLen = 0;
    m_maxHost = std::min<int>( hvec.size(), 255 );
    for( int i=0; i<m_maxHost; i++ )
    {
        m_dataLen += strlen( hvec[i]->first ) + 1;
    }

    auto data = new char[m_dataLen];
    auto ptr = data;
    for( int i=0; i<m_maxHost; i++ )
    {
        m_hostOffset[i] = ptr - data;
        auto len = strlen( hvec[i]->first );
        memcpy( ptr, hvec[i]->first, len+1 );
        ptr += len+1;
    }
    m_data = data;

    for( int i=0; i<m_maxHost; i++ )
    {
        m_hostLookup[i] = i;
    }
    std::sort( m_hostLookup, m_hostLookup+m_maxHost, [&hvec] ( const auto& l, const auto& r ) { return strcmp( hvec[l]->first, hvec[r]->first ) < 0; } );
}


#endif
