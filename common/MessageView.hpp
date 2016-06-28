#ifndef __MESSAGEVIEW_HPP__
#define __MESSAGEVIEW_HPP__

#include <assert.h>
#include <stdlib.h>
#include <string>

#include "../contrib/lz4/lz4.h"

#include "ExpandingBuffer.hpp"
#include "FileMap.hpp"
#include "RawImportMeta.hpp"

class MessageView
{
public:
    MessageView( const std::string& meta, const std::string& data )
        : m_meta( meta )
        , m_data( data )
    {
    }

    const char* operator[]( const size_t idx )
    {
        assert( idx < m_meta.Size() );
        const auto meta = m_meta[idx];
        auto buf = m_eb.Request( meta.size + 1 );
        const auto dec = LZ4_decompress_fast( m_data + meta.offset, buf, meta.size );
        assert( dec == meta[idx].compressedSize );
        buf[meta.size] = '\0';
        return buf;
    }

    size_t Size() const
    {
        return m_meta.Size() / sizeof( RawImportMeta );
    }

private:
    const FileMap<RawImportMeta> m_meta;
    const FileMap<char> m_data;

    ExpandingBuffer m_eb;
};

#endif
