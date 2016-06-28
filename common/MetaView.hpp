#ifndef __METAVIEW_HPP__
#define __METAVIEW_HPP__

#include <assert.h>
#include <stdlib.h>
#include <string>

#include "FileMap.hpp"

template<typename Meta, typename Data>
class MetaView
{
public:
    MetaView( const std::string& meta, const std::string& data )
        : m_meta( meta )
        , m_data( data )
    {
    }

    operator const Data*() const { return m_data; }

    const Data* operator[]( const size_t idx ) const
    {
        assert( idx < m_meta.Size() );
        return m_data + m_meta[idx] / sizeof( Data );
    }

    size_t Size() const
    {
        return m_meta.Size() / sizeof( Meta );
    }

private:
    const FileMap<Meta> m_meta;
    const FileMap<Data> m_data;
};

#endif
