#ifndef __BITSET_HPP__
#define __BITSET_HPP__

#include <stdlib.h>
#include <stdint.h>
#include <vector>

class BitSet
{
public:
    bool Set( bool enabled );
    bool Get( int pos ) const;
    int Size() const;

    void SetScoreData( int val ) { scoredata = val; }
    int GetScoreData() const { return scoredata; }

    std::vector<bool>* Convert();

private:
    enum { SizeBits = 6 };
    enum { InPlaceBits = 64 - 3 - SizeBits };
    enum : size_t { PtrMask = ~(size_t((1<<3)-1)) };

    union
    {
        struct
        {
            uint64_t external : 1;
            uint64_t scoredata : 2;
            uint64_t data : InPlaceBits;
            uint64_t size : SizeBits;
        };
        std::vector<bool>* ptr;
        uint64_t asNumber;
    };

    static_assert( ( 1 << SizeBits ) > InPlaceBits, "Not enough data bits for given size." );

    std::vector<bool>* GetPtr() const { return (std::vector<bool>*)(asNumber & PtrMask); }
};

static_assert( sizeof( BitSet ) == sizeof( uint64_t ), "BitSet size is greater than 8 bytes." );
static_assert( alignof( std::vector<bool> ) >= 8, "Vector pointers are aligned < 8." );

#endif
