#ifndef __MSGIDHASH_HPP__
#define __MSGIDHASH_HPP__

#include <stdint.h>

static inline int MsgIdHashBits( uint32_t size )
{
    // Load factor 0.75
    size *= 4;
    size /= 3;

    int r = 0;
    if( size & 0xFFFF0000 )
    {
        size >>= 16;
        r |= 16;
    }
    if( size & 0xFF00 )
    {
        size >>= 8;
        r |= 8;
    }
    if( size & 0xF0 )
    {
        size >>= 4;
        r |= 4;
    }
    if( size & 0xC )
    {
        size >>= 2;
        r |= 2;
    }
    if( size & 0x2 )
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
