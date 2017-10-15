#include <assert.h>

#include "BitSet.hpp"

bool BitSet::Set( bool enabled )
{
    if( !external )
    {
        if( size < InPlaceBits-1 )
        {
            if( enabled )
            {
                data |= uint64_t( 1 ) << size;
            }
            size++;
        }
        else
        {
            return false;
        }
    }
    else
    {
        GetPtr()->emplace_back( enabled );
    }
    return true;
}

std::vector<bool>* BitSet::Convert()
{
    assert( !external && size == InPlaceBits-1 );
    auto v = new std::vector<bool>();
    v->reserve( size+1 );
    for( int i=0; i<size; i++ )
    {
        v->emplace_back( ( data >> i ) & 0x1 );
    }
    auto score = scoredata;
    ptr = (std::vector<bool>*)((size_t)v | ( score << 1 ) | 1);
    assert( external );
    return v;
}

bool BitSet::Get( int pos ) const
{
    if( !external )
    {
        assert( pos < size );
        return ( data >> pos ) & 0x1;
    }
    else
    {
        assert( pos < GetPtr()->size() );
        return GetPtr()->operator[]( pos );
    }
}

int BitSet::Size() const
{
    if( !external )
    {
        return size;
    }
    else
    {
        return GetPtr()->size();
    }
}
