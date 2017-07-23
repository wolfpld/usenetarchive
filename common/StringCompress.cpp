#include <algorithm>
#include <assert.h>
#include <string.h>
#include <unordered_map>

#include "StringCompress.hpp"

StringCompress::StringCompress( const std::set<const char*, CharUtil::LessComparator>& strings )
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

StringCompress::StringCompress( const std::string& fn )
{
    FILE* f = fopen( fn.c_str(), "rb" );
    assert( f );

    fread( &m_dataLen, 1, sizeof( m_dataLen ), f );
    auto data = new char[m_dataLen];
    fread( data, 1, m_dataLen, f );
    m_data = data;
    fread( &m_maxHost, 1, sizeof( m_maxHost ), f );
    fread( m_hostLookup, 1, m_maxHost * sizeof( uint8_t ), f );
    fread( m_hostOffset, 1, m_maxHost * sizeof( uint32_t ), f );

    fclose( f );
}

StringCompress::~StringCompress()
{
    delete[] m_data;
}

size_t StringCompress::Pack( const char* in, uint8_t* out ) const
{
    const uint8_t* refout = out;

    while( *in != 0 )
    {
        if( *in == '@' )
        {
            auto test = in+1;
            auto it = std::lower_bound( m_hostLookup, m_hostLookup + m_maxHost, test, [this] ( const auto& l, const auto& r ) { return strcmp( m_data + m_hostOffset[l], r ) < 0; } );
            if( it != m_hostLookup + m_maxHost && strncmp( m_data + m_hostOffset[*it], test, 3 ) == 0 )
            {
                *out++ = 1;
                *out++ = (*it) + 1;
                break;
            }
        }
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

size_t StringCompress::Unpack( const uint8_t* in, char* out ) const
{
    const char* refout = out;

    while( *in != 0 )
    {
        if( *in == 1 )
        {
            in++;
            *out++ = '@';
            const char* dec = m_data + m_hostOffset[(*in) - 1];
            while( *dec != '\0' ) *out++ = *dec++;
            assert( *++in == 0 );
            break;
        }
        const char* dec = TrigramDecompressionModel[*in++];
        while( *dec != '\0' ) *out++ = *dec++;
    }

    *out++ = '\0';

    return out - refout;
}

void StringCompress::WriteData( const std::string& fn ) const
{
    FILE* f = fopen( fn.c_str(), "wb" );
    assert( f );

    fwrite( &m_dataLen, 1, sizeof( m_dataLen ), f );
    fwrite( m_data, 1, m_dataLen, f );
    fwrite( &m_maxHost, 1, sizeof( m_maxHost ), f );
    fwrite( m_hostLookup, 1, m_maxHost * sizeof( uint8_t ), f );
    fwrite( m_hostOffset, 1, m_maxHost * sizeof( uint32_t ), f );

    fclose( f );
}
