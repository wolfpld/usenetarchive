#include <assert.h>
#include <stdlib.h>
#include <time.h>

#include "../common/KillRe.hpp"
#include "../libuat/Archive.hpp"

#include "BottomBar.hpp"
#include "ThreadView.hpp"
#include "UTF8.hpp"

ThreadView::ThreadView( const Archive& archive, BottomBar& bottomBar )
    : View( 0, 1, 0, -2 )
    , m_archive( archive )
    , m_bottomBar( bottomBar )
    , m_data( archive.NumberOfMessages() )
    , m_tree( archive.NumberOfMessages() )
    , m_top( 0 )
    , m_cursor( 0 )
{
    unsigned int idx = 0;
    const auto toplevel = archive.GetTopLevel();
    for( int i=0; i<toplevel.size; i++ )
    {
        Fill( idx, toplevel.ptr[i], -1 );
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
    werase( m_win );
    Draw();
}

void ThreadView::Draw()
{
    int h = getmaxy( m_win );

    werase( m_win );

    const char* prev = nullptr;
    auto idx = m_top;
    for( int i=0; i<h; i++ )
    {
        assert( m_data[idx].valid == 1 );
        if( m_data[idx].parent == -1 ) prev = nullptr;
        DrawLine( idx, prev );
        if( m_data[idx].expanded )
        {
            idx++;
        }
        else
        {
            idx += m_archive.GetTotalChildrenCount( m_data[idx].msgid );
        }
        if( idx >= m_archive.NumberOfMessages() ) break;
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

void ThreadView::Expand( int cursor, bool recursive )
{
    m_data[cursor].expanded = 1;

    auto children = m_archive.GetChildren( m_data[cursor].msgid );
    int parent = cursor;
    cursor++;
    for( int i=0; i<children.size; i++ )
    {
        auto skip = m_archive.GetTotalChildrenCount( children.ptr[i] );
        if( !m_data[cursor].valid )
        {
            Fill( cursor, children.ptr[i], parent );
            bool line = i != children.size - 1;
            for( int j=0; j<skip; j++ )
            {
                m_tree[cursor+j].Set( line );
            }
        }
        if( recursive )
        {
            Expand( cursor, true );
        }
        cursor += skip;
    }
}

void ThreadView::Collapse( int cursor )
{
    m_data[cursor].expanded = 0;
}

void ThreadView::Fill( int index, int msgid, int parent )
{
    assert( m_data[index].valid == 0 );
    m_data[index].msgid = msgid;
    m_data[index].valid = 1;
    m_data[index].parent = parent;
}

static bool SameSubject( const char* subject, const char*& prev )
{
    if( subject == prev ) return true;
    if( prev == nullptr )
    {
        prev = KillRe( subject );
        return false;
    }
    auto oldprev = prev;
    prev = KillRe( subject );
    if( prev == oldprev ) return true;
    return strcmp( prev, oldprev ) == 0;
}

void ThreadView::DrawLine( int idx, const char*& prev )
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
    auto& tree = m_tree[idx];
    auto treecnt = tree.Size();
    len = w - 32 - dlen - treecnt*2;
    if( treecnt > 0 )
    {
        wattron( m_win, COLOR_PAIR(5) );
        for( int i=0; i<treecnt-1; i++ )
        {
            if( tree.Get( i ) )
            {
                waddch( m_win, ACS_VLINE );
            }
            else
            {
                waddch( m_win, ' ' );
            }
            waddch( m_win, ' ' );
        }
        if( tree.Get( treecnt-1 ) )
        {
            waddch( m_win, ACS_LTEE );
        }
        else
        {
            waddch( m_win, ACS_LLCORNER );
        }
        waddch( m_win, ACS_HLINE );
        wattroff( m_win, COLOR_PAIR(5) );
    }
    auto target = len;
    if( SameSubject( subject, prev ) )
    {
        len = 1;
        waddch( m_win, '>' );
    }
    else
    {
        end = utfendl( subject, len );
        wprintw( m_win, "%.*s", end - subject, subject );
    }
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
            while( !m_data[--m_cursor].valid );
            auto parent = m_data[m_cursor].parent;
            while( parent != -1 )
            {
                if( !m_data[parent].expanded )
                {
                    m_cursor = parent;
                }
                parent = m_data[parent].parent;
            }
        }
        while( ++offset );
    }

    Draw();
    m_bottomBar.Update( m_cursor + 1 );
    doupdate();
}
