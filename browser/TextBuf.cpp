#include "TextBuf.hpp"

enum { DataSize = 16*1024*1024 };

TextBuf::TextBuf()
    : m_data( new char[DataSize] )
    , m_ptr( m_data )
{
}

TextBuf::~TextBuf()
{
    delete[] m_data;
}

void TextBuf::Reset()
{
    m_ptr = m_data;
}

void TextBuf::Write( const char* src, int size )
{
    memcpy( m_ptr, src, size );
    m_ptr += size;
}
