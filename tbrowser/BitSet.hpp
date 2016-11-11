#ifndef __BITSET_HPP__
#define __BITSET_HPP__

#include <stdint.h>

class BitSet
{
public:
    BitSet();
    ~BitSet();

    void Set( bool enabled );
    bool Get( int pos ) const;
    int Size() const;

private:
    enum { InPlaceBits = 55 };

    union
    {
        struct
        {
            uint64_t inplace : 1;
            uint64_t data : InPlaceBits;
            uint64_t size : 8;
        };
        void* ptr;
        uint64_t asNumber;
    };
};

static_assert( sizeof( BitSet ) == sizeof( uint64_t ), "BitSet size is greater than 8 bytes." );

#endif
