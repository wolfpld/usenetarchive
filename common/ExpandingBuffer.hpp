#ifndef __EXPANDINGBUFFER_HPP__
#define __EXPANDINGBUFFER_HPP__

class ExpandingBuffer
{
public:
    ~ExpandingBuffer() { delete[] m_data; }

    char* Request( int size )
    {
        if( size > m_size )
        {
            delete[] m_data;
            m_data = new char[size];
            m_size = size;
        }
        return m_data;
    }

private:
    char* m_data = nullptr;
    int m_size = 0;
};

#endif
