#include <ctype.h>
#include <string.h>
#include <vector>

#include "../libuat/Archive.hpp"

#include "KillRe.hpp"

KillRe::KillRe()
{
    AddDefault();
}

KillRe::~KillRe()
{
    Clear();
}

void KillRe::Reset()
{
    Clear();
    AddDefault();
}

const char* KillRe::Kill( const char* str ) const
{
    for(;;)
    {
        if( *str == '\0' ) return str;
        while( *str == ' ' ) str++;
        auto match = m_prefix.begin();
        bool stop = false;
        while( !stop )
        {
            int idx = 0;
            const auto& matchstr = *match;
            for(;;)
            {
                if( matchstr[idx] == '\0' )
                {
                    str += idx;
                    stop = true;
                    break;
                }
                if( tolower( str[idx] ) != matchstr[idx] ) break;
                idx++;
            }
            if( !stop )
            {
                ++match;
                if( match == m_prefix.end() )
                {
                    return str;
                }
            }
        }
    }
}

void KillRe::Add( const char* str )
{
    Add( str, str + strlen( str ) );
}

void KillRe::Add( const char* begin, const char* end )
{
    const auto len = end - begin;
    auto str = new char[len+1];
    memcpy( str, begin, len );
    str[len] = '\0';
    m_prefix.emplace_back( str );
}

void KillRe::LoadPrefixList( const Archive& archive )
{
    auto prefix = archive.GetPrefixList();
    if( prefix.second != 0 )
    {
        auto ptr = prefix.first;
        auto fileend = ptr + prefix.second;
        while( ptr < fileend )
        {
            auto end = ptr;
            while( end < fileend && *end != '\n' && *end != '\r' ) end++;
            if( ptr != end )
            {
                Add( ptr, end );
            }
            while( end < fileend && ( *end == '\n' || *end == '\r' ) ) end++;
            ptr = end;
        }
    }
}

void KillRe::AddDefault()
{
    Add( "re:" );
    Add( "odp:" );
    Add( "re[2]:" );
    Add( "re[3]:" );
    Add( "re[4]:" );
    Add( "re[5]:" );
    Add( "re[6]:" );
    Add( "re[7]:" );
    Add( "re[8]:" );
    Add( "re[9]:" );
}

void KillRe::Clear()
{
    for( auto& v : m_prefix )
    {
        delete[] v;
    }
    m_prefix.clear();
}
