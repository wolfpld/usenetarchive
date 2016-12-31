#include <assert.h>

#include "BitSet.hpp"

void BitSet::Set( bool enabled )
{
    if( inplace )
    {
        if( size < InPlaceBits-1 )
        {
            data |= uint64_t( enabled ) << size;
            size++;
        }
        else
        {
            auto v = new std::vector<bool>();
            v->reserve( size+1 );
            for( int i=0; i<size; i++ )
            {
                v->emplace_back( ( data >> i ) & 0x1 );
            }
            v->emplace_back( enabled );
            auto score = scoredata;
            ptr = (std::vector<bool>*)((size_t)v | ( score << 1 ));
            assert( !inplace );
        }
    }
    else
    {
        GetPtr()->emplace_back( enabled );
    }
}

bool BitSet::Get( int pos ) const
{
    if( inplace )
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
    if( inplace )
    {
        return size;
    }
    else
    {
        return GetPtr()->size();
    }
}
