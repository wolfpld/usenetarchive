#include <assert.h>

#include "BitSet.hpp"

BitSet::BitSet()
    : asNumber( 1 )
{
}

BitSet::~BitSet()
{
}

void BitSet::Set( bool enabled )
{
    assert( size < InPlaceBits-1 );
    data &= enabled << size;
    size++;
}

bool BitSet::Get( int pos ) const
{
    assert( pos < size );
    return ( data >> pos ) & 0x1;
}

int BitSet::Size() const
{
    return size;
}
