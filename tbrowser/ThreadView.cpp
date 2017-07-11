#include <assert.h>
#include <stdlib.h>

#include "../common/Alloc.hpp"

#include "../libuat/Archive.hpp"

#include "MessageView.hpp"
#include "ThreadView.hpp"

ThreadView::ThreadView( const Archive& archive, PersistentStorage& storage, const Galaxy* galaxy, const MessageView& mview )
    : View( 0, 1, 0, -2 )
    , m_archive( &archive )
    , m_mview( mview )
    , m_tree( archive, storage, galaxy )
    , m_top( 0 )
    , m_cursor( 0 )
    , m_fillPos( 0 )
    , m_topLevelPos( 0 )
    , m_galaxy( galaxy != nullptr )
{
    Draw();
}

void ThreadView::Reset( const Archive& archive )
{
    m_archive = &archive;
    m_tree.Reset( archive );
    m_top = m_bottom = m_cursor = m_fillPos = m_topLevelPos = 0;
}

void ThreadView::Resize()
{
    if( m_mview.IsActive() )
    {
        int sw = getmaxx( stdscr );
        if( sw > 160 )
        {
            ResizeView( 0, 1, sw / 2, -2 );
        }
        else
        {
            int sh = getmaxy( stdscr ) - 2;
            ResizeView( 0, 1, 0, sh * 20 / 100 );
        }
    }
    else
    {
        ResizeView( 0, 1, 0, -2 );
    }
    werase( m_win );
    Draw();
    if( m_cursor >= m_bottom )
    {
        do
        {
            m_top = GetNext( m_top );
            m_bottom = GetNext( m_bottom );
        }
        while( m_cursor >= m_bottom );
        Draw();
    }
}

void ThreadView::Draw()
{
    int w, h;
    getmaxyx( m_win, h, w );

    werase( m_win );

    int cursorLine = -1;
    const char* prev = nullptr;
    auto idx = m_top;
    const int displayed = m_mview.IsActive() ? m_mview.DisplayedMessage() : -1;
    for( int i=0; i<h-1; i++ )
    {
        if( m_archive->GetParent( idx ) == -1 ) prev = nullptr;
        if( idx == m_cursor ) cursorLine = i;
        m_tree.DrawLine( m_win, i, idx, displayed == idx, displayed, m_cursor, prev );
        idx = GetNext( idx );
        if( idx >= m_archive->NumberOfMessages() ) break;
    }
    m_bottom = idx;

    char* tmp = (char*)alloca( w+1 );
    memset( tmp, ' ', w );
    tmp[w] = '\0';
    wmove( m_win, h-1, 0 );
    wattron( m_win, COLOR_PAIR( 11 ) | A_BOLD );
    wprintw( m_win, tmp );
    wmove( m_win, h-1, 0 );
    const auto allmsg = m_archive->NumberOfMessages();
    wprintw( m_win, " %i/%i", m_cursor+1, allmsg );
    wprintw( m_win, " (%.1f%%)", 100.f * (m_cursor+1) / allmsg );
    wattron( m_win, COLOR_PAIR( 1 ) );
    wprintw( m_win, " :: " );
    wattron( m_win, COLOR_PAIR( 11 ) );
    const auto root = GetRoot( m_cursor );
    const auto num = m_cursor - root + 1;
    const auto inthread = m_archive->GetTotalChildrenCount( root );
    wprintw( m_win, "Thread: %i/%i (%.1f%%)", num, inthread, 100.f * num / inthread );
    wattron( m_win, COLOR_PAIR( 1 ) );
    wprintw( m_win, " :: " );
    wattron( m_win, COLOR_PAIR( 11 ) );
    wprintw( m_win, "%i threads", m_archive->NumberOfTopLevel() );
    wattroff( m_win, COLOR_PAIR( 11 ) | A_BOLD );

    if( cursorLine != -1 )
    {
        wmove( m_win, cursorLine, 1 );
    }

    wnoutrefresh( m_win );
}

void ThreadView::PageForward()
{
    auto cnt = getmaxy( m_win ) - 2;
    while( cnt-- > 0 && m_bottom < m_archive->NumberOfMessages() )
    {
        m_top = GetNext( m_top );
        m_cursor = GetNext( m_cursor );
        m_bottom = GetNext( m_bottom );
    }
}

void ThreadView::PageBackward()
{
    auto cnt = getmaxy( m_win ) - 2;
    while( cnt-- > 0 && m_top > 0 )
    {
        m_top = GetPrev( m_top );
        m_cursor = GetPrev( m_cursor );
        m_bottom = GetPrev( m_bottom );
    }
}

void ThreadView::MoveCursorTop()
{
    m_cursor = m_top;
}

void ThreadView::MoveCursorBottom()
{
    m_cursor = GetPrev( m_bottom );
}

void ThreadView::FocusOn( int cursor )
{
    while( cursor >= m_bottom )
    {
        m_top = GetNext( m_top );
        m_bottom = GetNext( m_bottom );
    }
    while( cursor < m_top )
    {
        m_top = GetPrev( m_top );
        m_bottom = GetPrev( m_bottom );
    }
    const auto limit = m_archive->NumberOfMessages();
    if( m_bottom == limit )
    {
        Draw();
        return;
    }

    auto last = GetPrev( m_bottom );
    if( cursor == last )
    {
        m_top = GetNext( m_top );
        m_bottom = GetNext( m_bottom );
    }
    Draw();
}

void ThreadView::MoveCursor( int offset )
{
    auto end = m_archive->NumberOfMessages();

    if( offset > 0 )
    {
        do
        {
            auto prev = m_cursor;
            m_cursor = GetNext( m_cursor );
            if( m_cursor == end )
            {
                m_cursor = prev;
                break;
            }
            if( m_cursor == m_bottom )
            {
                m_top = GetNext( m_top );
                m_bottom = GetNext( m_bottom );
            }
        }
        while( --offset );
    }
    else if( offset < 0 )
    {
        do
        {
            if( m_cursor == 0 ) break;
            if( m_cursor == m_top )
            {
                m_cursor = m_top = GetPrev( m_top );
                m_bottom = GetPrev( m_bottom );
            }
            else
            {
                m_cursor = GetPrev( m_cursor );
            }
        }
        while( ++offset );
    }

    Draw();
}

void ThreadView::GoNextUnread()
{
    do
    {
        m_cursor = GetNext( m_cursor );
    }
    while( m_cursor != m_archive->NumberOfMessages() && m_tree.WasVisited( m_cursor ) );
    if( m_cursor == m_archive->NumberOfMessages() )
    {
        m_cursor = GetPrev( m_cursor );
    }
    FocusOn( m_cursor );
}

int ThreadView::GetNext( int idx )
{
    assert( idx < m_archive->NumberOfMessages() );
    if( IsExpanded( idx ) )
    {
        idx++;
    }
    else
    {
        idx += m_archive->GetTotalChildrenCount( idx );
    }
    return idx;
}

int ThreadView::GetPrev( int idx ) const
{
    assert( idx > 0 );
    idx--;
    auto parent = m_archive->GetParent( idx );
    while( parent != -1 )
    {
        if( !IsExpanded( parent ) )
        {
            idx = parent;
        }
        parent = m_archive->GetParent( parent );
    }
    return idx;
}
