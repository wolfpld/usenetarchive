#ifndef __FILEMAP_HPP__
#define __FILEMAP_HPP__

#include <stdio.h>
#include <string>

#include "Filesystem.hpp"
#include "mmap.hpp"

template<typename T>
class FileMap
{
public:
    FileMap( const std::string& fn, bool mayFail = false )
        : m_ptr( nullptr )
        , m_size( GetFileSize( fn.c_str() ) )
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

    FileMap( const FileMap& ) = delete;
    FileMap( FileMap&& src )
        : m_ptr( src.m_ptr )
        , m_size( src.m_size )
    {
        src.m_ptr = nullptr;
    }

    FileMap& operator=( const FileMap& ) = delete;
    FileMap& operator=( FileMap&& )
    {
        m_ptr = src.m_ptr;
        m_size = src.m_size;
        src.m_ptr = nullptr;
        return *this;
    }

    ~FileMap()
    {
        if( m_ptr )
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
};

#endif
