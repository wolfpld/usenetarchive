#include <ctype.h>
#include <string>
#include <vector>

#include "KillRe.hpp"

std::vector<std::string> ReList = {
    "re:",
    "odp:",
    "re[2]:",
    "re[3]:",
    "re[4]:",
    "re[5]:",
    "re[6]:",
    "re[7]:",
    "re[8]:",
    "re[9]:"
};

const char* KillRe( const char* str )
{
    for(;;)
    {
        if( *str == '\0' ) return str;
        while( *str == ' ' ) str++;
        auto match = ReList.begin();
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
                if( match == ReList.end() )
                {
                    return str;
                }
            }
        }
    }
}

void AddToReList( const char* str )
{
    ReList.emplace_back( str );
}
