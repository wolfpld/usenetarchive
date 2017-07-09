#ifndef __THREADDATA_HPP__
#define __THREADDATA_HPP__

#include <stdint.h>

enum { CondensedDepthThreshold = 20 };
enum { CondensedBits = 4 };
enum { CondensedMax = ( 1 << CondensedBits ) - 1 };
enum { CondensedStep = 4 };

struct ThreadData
{
    uint8_t expanded   : 1;
    uint8_t valid      : 1;
    uint8_t visited    : 1;
    uint8_t visall     : 1;
    uint8_t condensed  : CondensedBits;
    uint8_t galaxy     : 3;
};

static_assert( sizeof( ThreadData ) == sizeof( uint16_t ), "Thread data size greater than 2 byte." );

enum class ScoreState
{
    Unknown,
    Neutral,
    Positive,
    Negative
};

#endif
