#ifndef __STRING_COMPRESSION_MODEL__
#define __STRING_COMPRESSION_MODEL__

#include <set>
#include <stdint.h>
#include <string>
#include <vector>

#include "CharUtil.hpp"

class StringCompress
{
public:
    StringCompress( const std::set<const char*, CharUtil::LessComparator>& strings );
    StringCompress( const std::string& fn );
    ~StringCompress();

    size_t Pack( const char* in, uint8_t* out ) const;
    size_t Unpack( const uint8_t* in, char* out ) const;

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

#endif
