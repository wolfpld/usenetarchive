#ifndef __PARSEDATE_HPP__
#define __PARSEDATE_HPP__

#include <stdint.h>
#include <time.h>
#include <vector>

struct ParseDateStats
{
    uint32_t baddate;
    uint32_t recdate;
    uint32_t timetravel;
};

time_t ParseDate( const char* post, ParseDateStats& stats, std::vector<const char*>& cache );

#endif
