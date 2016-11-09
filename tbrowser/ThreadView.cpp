#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include "../libuat/Archive.hpp"

#include "BottomBar.hpp"
#include "ThreadView.hpp"
#include "UTF8.hpp"

ThreadView::ThreadView( const Archive& archive, BottomBar& bottomBar )
    : View( 0, 1, 0, -2 )
    , m_archive( archive )
    , m_bottomBar( bottomBar )
    , m_data( archive.NumberOfMessages() )
    , m_top( 0 )
    , m_cursor( 0 )
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

void ThreadView::Resize()
{
    ResizeView( 0, 1, 0, -2 );
    wclear( m_win );
    Draw();
}

void ThreadView::Draw()
{
    int h = getmaxy( m_win );

    werase( m_win );

    auto idx = m_top;
    for( int i=0; i<h; i++ )
    {
        assert( m_data[idx].valid == 1 );
        DrawLine( idx );
        if( m_data[idx].expanded )
        {
            idx++;
        }
        else
        {
            idx += m_archive.GetTotalChildrenCount( m_data[idx].msgid );
        }
    }

    wnoutrefresh( m_win );
}

void ThreadView::Up()
{
    MoveCursor( -1 );
}

void ThreadView::Down()
{
    MoveCursor( 1 );
}

void ThreadView::Fill( int index, int msgid )
{
    assert( m_data[index].valid == 0 );
    m_data[index].msgid = msgid;
    m_data[index].valid = 1;
}

void ThreadView::DrawLine( int idx )
{
    const auto midx = m_data[idx].msgid;
    if( m_cursor == idx )
    {
        wattron( m_win, COLOR_PAIR(2) | A_BOLD );
        wprintw( m_win, "->" );
        wattroff( m_win, COLOR_PAIR(2) | A_BOLD );
    }
    else
    {
        wprintw( m_win, "  " );
    }

    const auto children = m_archive.GetTotalChildrenCount( midx );
    if( children > 9999 )
    {
        wprintw( m_win, "++++ [" );
    }
    else
    {
        wprintw( m_win, "%4i [", children );
    }

    auto realname = m_archive.GetRealName( midx );
    wattron( m_win, COLOR_PAIR(3) | A_BOLD );
    int len = 18;
    auto end = utfendl( realname, len );
    wprintw( m_win, "%.*s", end - realname, realname );
    while( len++ < 18 )
    {
        waddch( m_win, ' ' );
    }
    wattroff( m_win, COLOR_PAIR(3) | A_BOLD );
    wprintw( m_win, "] " );

    if( children == 1 )
    {
        wprintw( m_win, "  " );
    }
    else
    {
        wattron( m_win, COLOR_PAIR(4) );
        waddch( m_win, m_data[idx].expanded ? '-' : '+' );
        waddch( m_win, ' ' );
        wattroff( m_win, COLOR_PAIR(4) );
    }

    time_t date = m_archive.GetDate( midx );
    auto lt = localtime( &date );
    char buf[64];
    auto dlen = strftime( buf, 64, "%F %R", lt );

    auto w = getmaxx( m_win );
    auto subject = m_archive.GetSubject( midx );
    len = w - 32 - dlen;
    auto target = len;
    end = utfendl( subject, len );
    wprintw( m_win, "%.*s", end - subject, subject );
    while( len++ < target )
    {
        waddch( m_win, ' ' );
    }
    waddch( m_win, '[' );
    wattron( m_win, COLOR_PAIR(2) );
    wprintw( m_win, "%s", buf );
    wattroff( m_win, COLOR_PAIR(2) );
    waddch( m_win, ']' );
}

void ThreadView::MoveCursor( int offset )
{
    auto end = m_archive.NumberOfMessages();

    if( offset > 0 )
    {
        do
        {
            auto prev = m_cursor;
            if( m_data[m_cursor].expanded )
            {
                m_cursor++;
            }
            else
            {
                m_cursor += m_archive.GetTotalChildrenCount( m_data[m_cursor].msgid );
            }
            if( m_cursor == end )
            {
                m_cursor = prev;
                break;
            }
        }
        while( --offset );
    }
    else if( offset < 0 )
    {
        do
        {
            if( m_cursor == 0 ) break;
        }
        while( ++offset );
    }

    Draw();
    m_bottomBar.Update( m_cursor + 1 );
    doupdate();
}
