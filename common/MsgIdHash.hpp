#ifndef __MSGIDHASH_HPP__
#define __MSGIDHASH_HPP__

#include <assert.h>
#include <stdint.h>

// load factor in 1/100
static inline int MsgIdHashBits( uint32_t size, uint8_t load )
{
    assert( load <= 100 );

    auto s = uint64_t( size ) * 100;
    s /= load;

    int r = 0;
    if( s & 0xFFFF0000 )
    {
        s >>= 16;
        r |= 16;
    }
    if( s & 0xFF00 )
    {
        s >>= 8;
        r |= 8;
    }
    if( s & 0xF0 )
    {
        s >>= 4;
        r |= 4;
    }
    if( s & 0xC )
    {
        s >>= 2;
        r |= 2;
    }
    if( s & 0x2 )
    {
        r |= 1;
    }
    return r + 1;
}

static inline int MsgIdHashSize( int bits )
{
    return 1 << bits;
}

static inline int MsgIdHashMask( int bits )
{
    return MsgIdHashSize( bits ) - 1;
}

#endif
