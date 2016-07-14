#ifndef __TEXTBUF_HPP__
#define __TEXTBUF_HPP__

#include <string.h>

class TextBuf
{
public:
    TextBuf();
    ~TextBuf();

    void Reset();
    void Write( const char* src, int size );
    void Write( const char* src ) { Write( src, strlen( src ) ); }
    void PutC( char c ) { *m_ptr++ = c; }

    operator const char*() const { *m_ptr = '\0'; return m_data; }

private:
    char* m_data;
    char* m_ptr;
};

#endif
