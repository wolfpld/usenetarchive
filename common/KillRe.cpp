#include <string>
#include <vector>

#include "KillRe.hpp"

std::vector<std::string> ReList = {
    "Re:",
    "RE:",
    "re:",
    "Odp:",
    "Re[2]:",
    "Re[3]:",
    "Re[4]:",
    "Re[5]:",
    "Re[6]:",
    "Re[7]:",
    "Re[8]:",
    "Re[9]:"
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
                if( str[idx] != matchstr[idx] ) break;
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
