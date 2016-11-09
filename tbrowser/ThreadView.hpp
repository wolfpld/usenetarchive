#ifndef __THREADVIEW_HPP__
#define __THREADVIEW_HPP__

#include <stdint.h>
#include <vector>

#include "View.hpp"

class Archive;

struct ThreadData
{
    unsigned int expanded   : 1;
    unsigned int valid      : 1;
    unsigned int msgid      : 30;
};

static_assert( sizeof( ThreadData ) == sizeof( uint32_t ), "Wrong size of ThreadData" );

class ThreadView : public View
{
public:
    ThreadView( const Archive& archive );
    ~ThreadView();

    void Resize();

    void Draw();

    int GetCursor() const { return m_cursor; }

private:
    void Fill( int index, int msgid );
    void DrawLine( int idx );

    const Archive& m_archive;
    std::vector<ThreadData> m_data;
    int m_top;
    int m_cursor;
};

#endif
