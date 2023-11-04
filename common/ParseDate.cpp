#include <algorithm>
#include <assert.h>
#include <limits>
#include <stdlib.h>
#include <string.h>

#include "MessageLogic.hpp"
#include "ParseDate.hpp"
#include "String.hpp"

extern "C" { time_t parsedate_rfc5322_lax(const char *date); }

constexpr uint32_t TimeTravelLimit = 60*60*24*7;    // one week

struct TimeGroup
{
    uint32_t timestamp;
    uint32_t count;
};


time_t ParseDate( const char* post, ParseDateStats& stats, std::vector<const char*>& cache )
{
    char tmp[1024];

    cache.clear();
    auto buf = post;

    while( *buf != '\n' )
    {
        if( strnicmpl( buf, "received: ", 10 ) == 0 )
        {
            buf += 10;
            while( *buf != ';' && *buf != '\n' ) buf++;
            if( *buf == ';' )
            {
                buf++;
                while( *buf == ' ' || *buf == '\t' ) buf++;
                cache.emplace_back( buf );
            }
        }
        while( *buf++ != '\n' ) {}
    }

    buf = FindOptionalHeader( post, "nntp-posting-date: ", 19 );
    if( *buf != '\n' )
    {
        buf += 19;
        if( *buf != '\n' )
        {
            cache.emplace_back( buf );
        }
    }
    buf = FindOptionalHeader( post, "injection-date: ", 16 );
    if( *buf != '\n' )
    {
        buf += 16;
        if( *buf != '\n' )
        {
            cache.emplace_back( buf );
        }
    }

    time_t recvdate = -1;
    if( cache.size() == 1 )
    {
        recvdate = parsedate_rfc5322_lax( cache[0] );
    }
    else if( cache.size() > 1 )
    {
        std::vector<uint32_t> timestamps;
        timestamps.reserve( cache.size() );
        for( auto& v : cache )
        {
            auto ts = parsedate_rfc5322_lax( v );
            if( ts != -1 )
            {
                timestamps.emplace_back( ts );
            }
        }
        if( timestamps.size() == 1 )
        {
            recvdate = timestamps[0];
        }
        else if( timestamps.size() > 1 )
        {
            assert( timestamps.size() < std::numeric_limits<uint16_t>::max() );
            std::vector<uint16_t> sort;
            sort.reserve( timestamps.size() );
            for( int i=0; i<timestamps.size(); i++ )
            {
                sort.emplace_back( i );
            }
            std::sort( sort.begin(), sort.end(), [&timestamps] ( const auto& l, const auto& r ) { return timestamps[l] < timestamps[r]; } );

            std::vector<TimeGroup> groups;
            groups.emplace_back( TimeGroup { timestamps[sort[0]], 1 } );
            for( int i=1; i<timestamps.size(); i++ )
            {
                assert( timestamps[sort[i]] >= groups.back().timestamp );
                if( timestamps[sort[i]] - groups.back().timestamp > TimeTravelLimit )
                {
                    groups.emplace_back( TimeGroup { timestamps[sort[i]], 1 } );
                }
                else
                {
                    groups.back().count++;
                }
            }
            if( groups.size() > 1 )
            {
                std::sort( groups.begin(), groups.end(), [] ( const auto& l, const auto& r ) {
                    if( l.count == r.count )
                    {
                        return l.timestamp < r.timestamp;
                    }
                    else
                    {
                        return l.count > r.count;
                    }
                } );
            }
            recvdate = groups[0].timestamp;
        }
    }

    time_t date = -1;
    buf = FindOptionalHeader( post, "date: ", 6 );
    if( *buf == '\n' )
    {
        buf = FindOptionalHeader( post, "date:\t", 6 );
    }
    if( *buf != '\n' )
    {
        buf += 6;
        auto end = buf;
        while( *end != '\n' && *end != '\0' ) end++;
        if( *end == '\n' )
        {
            const auto size = end - buf;
            memcpy( tmp, buf, size );
            tmp[size] = '\0';
            for( int i=0; i<size; i++ )
            {
                if( tmp[i] == '-' ) tmp[i] = ' ';
            }
            date = parsedate_rfc5322_lax( tmp );
        }
    }

    if( date == -1 )
    {
        stats.baddate++;
        if( recvdate == -1 )
        {
            struct tm tm = {};
            if( sscanf( buf, "%d/%d/%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday ) == 3 )
            {
                if( tm.tm_year >= 1970 && tm.tm_mon <= 12 && tm.tm_mday <= 31 )
                {
                    tm.tm_year -= 1900;
                    tm.tm_mon--;
                    date = mktime( &tm );
                }
                else
                {
                    date = 0;
                }
            }
            else
            {
                date = 0;
            }
        }
        else
        {
            stats.recdate++;
            date = recvdate;
        }
    }
    else if( recvdate != -1 )
    {
        if( abs( date - recvdate ) > TimeTravelLimit )
        {
            stats.timetravel++;
            date = recvdate;
        }
    }

    return date;
}
