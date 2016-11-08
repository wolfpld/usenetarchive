#include <assert.h>
#include "../libuat/Archive.hpp"

#include "ThreadView.hpp"

ThreadView::ThreadView( const Archive& archive )
    : View( 0, 1, -1, -1 )
    , m_archive( archive )
    , m_data( archive.NumberOfMessages() )
    , m_top( 0 )
{
    unsigned int idx = 0;
    const auto toplevel = archive.GetTopLevel();
    for( int i=0; i<toplevel.size; i++ )
    {
        Fill( idx, toplevel.ptr[i] );
        idx += archive.GetTotalChildrenCount( toplevel.ptr[i] );
    }

    Draw();
}

ThreadView::~ThreadView()
{
}

void ThreadView::Draw()
{
    int w, h;
    getmaxyx( m_win, h, w );

    werase( m_win );

    auto idx = m_top;
    for( int i=0; i<h; i++ )
    {
        assert( m_data[idx].valid == 1 );
        wprintw( m_win, "%s\n", m_archive.GetSubject( m_data[idx].msgid ) );
        if( m_data[idx].expanded )
        {
            idx++;
        }
        else
        {
            idx += m_archive.GetTotalChildrenCount( m_data[idx].msgid );
        }
    }

    wrefresh( m_win );
}

void ThreadView::Fill( int index, int msgid )
{
    assert( m_data[index].valid == 0 );
    m_data[index].msgid = msgid;
    m_data[index].valid = 1;
}
