#ifndef __ZMESSAGEVIEW_HPP__
#define __ZMESSAGEVIEW_HPP__

#include <assert.h>
#include <stdlib.h>
#include <string>

#define ZSTD_STATIC_LINKING_ONLY
#include "../contrib/zstd/zstd.h"

#include "ExpandingBuffer.hpp"
#include "FileMap.hpp"
#include "RawImportMeta.hpp"

class ZMessageView
{
public:
    ZMessageView( const std::string& meta, const std::string& data, const std::string& dict )
        : m_meta( meta )
        , m_data( data )
        , m_ctx( ZSTD_createDCtx() )
    {
        const FileMap<char> dictdata( dict );
        m_dict = ZSTD_createDDict_byReference( dictdata, dictdata.Size() );
    }

    ZMessageView( const FileMapPtrs& meta, const FileMapPtrs& data, const FileMapPtrs& dict )
        : m_meta( meta )
        , m_data( data )
        , m_ctx( ZSTD_createDCtx() )
    {
        const FileMap<char> dictdata( dict );
        m_dict = ZSTD_createDDict_byReference( dictdata, dictdata.Size() );
    }

    ~ZMessageView()
    {
        ZSTD_freeDCtx( m_ctx );
        ZSTD_freeDDict( m_dict );
    }

    const char* operator[]( const size_t idx )
    {
        assert( idx < m_meta.Size() );
        const auto meta = m_meta[idx];
        auto buf = m_eb.Request( meta.size + 1 );
        const auto dec = ZSTD_decompress_usingDDict( m_ctx, buf, meta.size, m_data + meta.offset, meta.compressedSize, m_dict );
        assert( dec == meta.size );
        buf[meta.size] = '\0';
        return buf;
    }

    struct RawMessage
    {
        const char* ptr;
        size_t size;
        size_t compressedSize;
    };

    RawMessage Raw( const size_t idx ) const
    {
        const auto meta = m_meta[idx];
        return RawMessage { m_data + meta.offset, meta.size, meta.compressedSize };
    }

    struct Ptrs
    {
        const RawImportMeta* meta;
        const char* data;
        size_t metasize, datasize;
    };

    Ptrs Pointers() const
    {
        return Ptrs { m_meta, m_data, m_meta.Size(), m_data.Size() };
    }

    size_t Size() const
    {
        return m_meta.Size() / sizeof( RawImportMeta );
    }

private:
    const FileMap<RawImportMeta> m_meta;
    const FileMap<char> m_data;

    ZSTD_DCtx* m_ctx;
    ZSTD_DDict* m_dict;

    ExpandingBuffer m_eb;
};

#endif
