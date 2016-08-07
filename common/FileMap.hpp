#ifndef __FILEMAP_HPP__
#define __FILEMAP_HPP__

#include <stdio.h>
#include <string>

#include "Filesystem.hpp"
#include "mmap.hpp"

struct FileMapPtrs
{
    const char* ptr;
    uint64_t size;
};

template<typename T>
class FileMap
{
public:
    FileMap( const std::string& fn, bool mayFail = false )
        : m_ptr( nullptr )
        , m_size( GetFileSize( fn.c_str() ) )
        , m_release( true )
    {
        FILE* f = fopen( fn.c_str(), "rb" );
        if( !f )
        {
            if( mayFail ) return;
            fprintf( stderr, "Cannot open %s\n", fn.c_str() );
            exit( 1 );
        }
        m_ptr = (T*)mmap( nullptr, m_size, PROT_READ, MAP_SHARED, fileno( f ), 0 );
        fclose( f );
    }

    FileMap( const FileMapPtrs& ptrs )
        : m_ptr( (T*)ptrs.ptr )
        , m_size( ptrs.size )
        , m_release( false )
    {
    }

    FileMap( const FileMap& ) = delete;
    FileMap( FileMap&& src )
        : m_ptr( src.m_ptr )
        , m_size( src.m_size )
        , m_release( src.m_release )
    {
        src.m_ptr = nullptr;
    }

    FileMap& operator=( const FileMap& ) = delete;
    FileMap& operator=( FileMap&& src )
    {
        m_ptr = src.m_ptr;
        m_size = src.m_size;
        m_release = src.m_release;
        src.m_ptr = nullptr;
        return *this;
    }

    ~FileMap()
    {
        if( m_release && m_ptr )
        {
            munmap( m_ptr, m_size );
        }
    }

    operator const T*() const { return m_ptr; }
    uint64_t Size() const { return m_size; }
    uint64_t DataSize() const { return m_size / sizeof( T ); }

private:
    T* m_ptr;
    uint64_t m_size;
    bool m_release;
};

#endif
