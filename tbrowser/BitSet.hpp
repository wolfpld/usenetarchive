#ifndef __BITSET_HPP__
#define __BITSET_HPP__

#include <stdint.h>
#include <vector>

class BitSet
{
public:
    BitSet() : asNumber( 1 ) {}
    ~BitSet() { if( !inplace ) delete ptr; }

    void Set( bool enabled );
    bool Get( int pos ) const;
    int Size() const;

private:
    enum { SizeBits = 6 };
    enum { InPlaceBits = 64 - 1 - SizeBits };

    union
    {
        struct
        {
            uint64_t inplace : 1;
            uint64_t data : InPlaceBits;
            uint64_t size : SizeBits;
        };
        std::vector<bool>* ptr;
        uint64_t asNumber;
    };

    static_assert( ( 1 << SizeBits ) > InPlaceBits, "Not enough data bits for given size." );
};

static_assert( sizeof( BitSet ) == sizeof( uint64_t ), "BitSet size is greater than 8 bytes." );
static_assert( alignof( std::vector<bool> ) > 1, "Vector pointers are aligned to 1." );

#endif
