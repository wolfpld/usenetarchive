#ifndef __REFERENCESPARENT_HPP__
#define __REFERENCESPARENT_HPP__

#include <assert.h>
#include <string>
#include <vector>

#include "MessageLogic.hpp"
#include "StringCompress.hpp"

inline const char* FindReferences( const char* msg )
{
    auto buf = FindOptionalHeader( msg, "references: ", 12 );
    if( *buf != '\n' ) return buf + 12;
    buf = FindOptionalHeader( msg, "in-reply-to: ", 13 );
    if( *buf != '\n' ) return buf + 13;
    return buf;
}

inline bool ValidateMsgId( const char* begin, const char* end, char* dst )
{
    bool broken = false;
    while( begin != end )
    {
        if( *begin != ' ' && *begin != '\t' )
        {
            *dst++ = *begin;
        }
        else
        {
            broken = true;
        }
        begin++;
    }
    *dst++ = '\0';
    return broken;
}

// Return message index of parent.
//  -1 indicates no parent
//  -2 indicates broken, unrecoverable reference information
template<class Search>
inline int GetParentFromReferences( const char* post, const StringCompress& compress, const Search& hash, char* tmp )
{
    auto buf = FindReferences( post );
    if( *buf == '\n' ) return -1;

    const auto terminate = buf;
    while( *buf != '\n' ) buf++;
    if( buf == terminate ) return -1;
    buf--;
    // Handle "In-Reply-To: <msg@id> from a@x.y at 12/12/1999"
    while( buf > terminate && ( *buf != '>' && *buf != '<' ) ) buf--;
    if( buf <= terminate || *buf == '<' ) return -2;
    for(;;)
    {
        while( buf > terminate && ( *buf == ' ' || *buf == '\t' ) ) buf--;
        if( buf <= terminate || *buf != '>' )
        {
            return -2;
        }
        auto end = buf;
        buf--;
        while( buf > terminate && *buf != '<' ) buf--;
        if( *buf != '<' )
        {
            return -2;
        }
        buf++;
        assert( end - buf < 1024 );
        if( !IsMsgId( buf, end ) )
        {
            return -2;
        }

        ValidateMsgId( buf, end, tmp );

        uint8_t pack[1024];
        compress.Pack( tmp, pack );
        auto idx = hash.Search( pack );
        if( idx >= 0 )
        {
            return idx;
        }

        buf -= 2;
    }
}

std::vector<std::string> GetAllReferences( const char* post, const StringCompress& compress )
{
    std::vector<std::string> ret;

    auto buf = FindReferences( post );
    if( *buf == '\n' ) return ret;

    const auto terminate = buf;
    while( *buf != '\n' ) buf++;
    if( buf == terminate ) return ret;
    buf--;
    // Handle "In-Reply-To: <msg@id> from a@x.y at 12/12/1999"
    while( buf > terminate && ( *buf != '>' && *buf != '<' ) ) buf--;
    if( buf <= terminate || *buf == '<' ) return ret;
    for(;;)
    {
        while( buf > terminate && ( *buf == ' ' || *buf == '\t' ) ) buf--;
        if( buf <= terminate || *buf != '>' )
        {
            return ret;
        }
        auto end = buf;
        buf--;
        while( buf > terminate && *buf != '<' ) buf--;
        if( *buf != '<' )
        {
            return ret;
        }
        buf++;
        assert( end - buf < 1024 );
        if( !IsMsgId( buf, end ) )
        {
            return ret;
        }

        char tmp[1024];
        ValidateMsgId( buf, end, tmp );

        uint8_t pack[1024];
        compress.Pack( tmp, pack );
        ret.emplace_back( (char*)pack );

        buf -= 2;
    }
}

#endif
