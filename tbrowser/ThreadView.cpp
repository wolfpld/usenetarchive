#include <assert.h>
#include <limits>
#include <stdlib.h>
#include <time.h>

#include "../common/Alloc.hpp"
#include "../common/KillRe.hpp"
#include "../libuat/Archive.hpp"
#include "../libuat/PersistentStorage.hpp"

#include "MessageView.hpp"
#include "ThreadView.hpp"
#include "UTF8.hpp"

ThreadView::ThreadView( const Archive& archive, PersistentStorage& storage, const MessageView& mview )
    : View( 0, 1, 0, -2 )
    , m_archive( archive )
    , m_storage( storage )
    , m_mview( mview )
    , m_data( archive.NumberOfMessages() )
    , m_tree( archive.NumberOfMessages() )
    , m_top( 0 )
    , m_cursor( 0 )
    , m_fillPos( 0 )
    , m_topLevelPos( 0 )
{
    Draw();
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
    for( int i=0; i<h-1; i++ )
    {
        if( m_archive.GetParent( idx ) == -1 ) prev = nullptr;
        if( DrawLine( i, idx, prev ) )
        {
            cursorLine = i;
        }
        idx = GetNext( idx );
        if( idx >= m_archive.NumberOfMessages() ) break;
    }
    m_bottom = idx;

    char* tmp = (char*)alloca( w+1 );
    memset( tmp, ' ', w );
    tmp[w] = '\0';
    wmove( m_win, h-1, 0 );
    wattron( m_win, COLOR_PAIR( 11 ) | A_BOLD );
    wprintw( m_win, tmp );
    wmove( m_win, h-1, 0 );
    wprintw( m_win, " [%i/%i]", m_cursor+1, m_archive.NumberOfMessages() );
    wattron( m_win, COLOR_PAIR( 1 ) );
    wprintw( m_win, " :: " );
    wattroff( m_win, A_BOLD );
    wprintw( m_win, "%i threads", m_archive.NumberOfTopLevel() );
    wattroff( m_win, COLOR_PAIR( 1 ) );

    if( cursorLine != -1 )
    {
        wmove( m_win, cursorLine, 1 );
    }

    wnoutrefresh( m_win );
}

bool ThreadView::CanExpand( int cursor )
{
    return m_archive.GetTotalChildrenCount( cursor ) > 1;
}

void ThreadView::Expand( int cursor, bool recursive )
{
    m_data[cursor].valid = 1;
    m_data[cursor].expanded = 1;

    auto children = m_archive.GetChildren( cursor );
    int parent = cursor;
    cursor++;
    for( int i=0; i<children.size; i++ )
    {
        auto skip = m_archive.GetTotalChildrenCount( children.ptr[i] );
        if( !m_data[cursor].valid )
        {
            m_data[cursor].valid = 1;
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

int ThreadView::GetRoot( int cursor ) const
{
    while( m_archive.GetParent( cursor ) != -1 ) cursor = m_archive.GetParent( cursor );
    return cursor;
}

void ThreadView::PageForward()
{
    auto cnt = getmaxy( m_win ) - 2;
    while( cnt-- > 0 && m_bottom < m_archive.NumberOfMessages() )
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

void ThreadView::ExpandFill( int cursor )
{
    if( cursor == m_archive.NumberOfMessages()-1 || m_data[cursor+1].valid ) return;
    auto children = m_archive.GetChildren( cursor );
    int parent = cursor;
    cursor++;
    for( int i=0; i<children.size; i++ )
    {
        auto skip = m_archive.GetTotalChildrenCount( children.ptr[i] );
        assert( !m_data[cursor].valid );
        m_data[cursor].valid = true;
        bool line = i != children.size - 1;
        for( int j=0; j<skip; j++ )
        {
            m_tree[cursor+j].Set( line );
        }
        ExpandFill( cursor );
        cursor += skip;
    }
}

void ThreadView::Collapse( int cursor )
{
    m_data[cursor].expanded = 0;
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
    const auto limit = m_archive.NumberOfMessages();
    if( m_bottom == limit ) return;

    auto last = GetPrev( m_bottom );
    if( cursor == last )
    {
        m_top = GetNext( m_top );
        m_bottom = GetNext( m_bottom );
    }
    Draw();
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

bool ThreadView::DrawLine( int line, int idx, const char*& prev )
{
    bool hilite = m_mview.IsActive() && m_mview.DisplayedMessage() == idx;
    bool wasVisited = CheckVisited( idx );

    if( hilite ) wattron( m_win, COLOR_PAIR(2) | A_BOLD );
    if( m_cursor == idx )
    {
        if( !hilite ) wattron( m_win, COLOR_PAIR(2) | A_BOLD );
        wmove( m_win, line, 0 );
        wprintw( m_win, "->" );
        if( !hilite ) wattroff( m_win, COLOR_PAIR(2) | A_BOLD );
    }
    else
    {
        wmove( m_win, line, 2 );
    }

    if( wasVisited )
    {
        bool complete = m_data[idx].visall;
        if( !complete )
        {
            ExpandFill( idx );
            complete = true;
            std::vector<uint32_t> stack;
            stack.reserve( 4 * 1024 );
            stack.push_back( idx );
            while( !stack.empty() )
            {
                const auto id = stack.back();
                if( !CheckVisited( id ) )
                {
                    complete = false;
                    break;
                }
                stack.pop_back();
                const auto children = m_archive.GetChildren( id );
                for( int i=0; i<children.size; i++ )
                {
                    if( !m_data[children.ptr[i]].visall )
                    {
                        stack.emplace_back( children.ptr[i] );
                    }
                }
            }
            if( complete )
            {
                m_data[idx].visall = true;
            }
        }
        if( complete )
        {
            waddch( m_win, 'R' );
        }
        else
        {
            waddch( m_win, 'r' );
        }
    }
    else
    {
        waddch( m_win, '-' );
    }

    const auto children = m_archive.GetTotalChildrenCount( idx );
    if( children > 9999 )
    {
        wprintw( m_win, "++++ [" );
    }
    else
    {
        wprintw( m_win, "%4i [", children );
    }

    auto realname = m_archive.GetRealName( idx );
    if( !hilite ) wattron( m_win, COLOR_PAIR(3) | A_BOLD );
    int len = 18;
    auto end = utfendl( realname, len );
    utfprint( m_win, realname, end );
    if( len < 18 )
    {
        wmove( m_win, line, 27 );
    }
    if( !hilite ) wattroff( m_win, COLOR_PAIR(3) | A_BOLD );
    waddch( m_win, ']' );

    if( children > 1 )
    {
        if( !hilite ) wattron( m_win, COLOR_PAIR(4) );
        wmove( m_win, line, 29 );
        waddch( m_win, m_data[idx].expanded ? '-' : '+' );
        if( !hilite ) wattroff( m_win, COLOR_PAIR(4) );
    }

    time_t date = m_archive.GetDate( idx );
    auto lt = localtime( &date );
    char buf[64];
    auto dlen = strftime( buf, 64, "%F %R", lt );

    wmove( m_win, line, 31 );
    auto w = getmaxx( m_win );
    auto subject = m_archive.GetSubject( idx );
    auto& tree = m_tree[idx];
    auto treecnt = tree.Size();
    len = w - 33 - dlen;
    if( treecnt > 0 )
    {
        int childline = std::numeric_limits<int>::max();
        if( m_mview.IsActive() && !hilite )
        {
            int parent = m_archive.GetParent( idx );
            int cnt = 1;
            while( parent != -1 )
            {
                if( parent == m_mview.DisplayedMessage() )
                {
                    childline = treecnt - cnt;
                    break;
                }
                cnt++;
                parent = m_archive.GetParent( parent );
            }
        }

        len -= 2;
        if( !hilite ) wattron( m_win, COLOR_PAIR(5) );
        int i;
        for( i=0; i<treecnt-1 && len > 1; i++ )
        {
            if( childline == i )
            {
                wattroff( m_win, COLOR_PAIR(5) );
                wattron( m_win, COLOR_PAIR(4) );
            }
            if( tree.Get( i ) )
            {
                wmove( m_win, line, 31 + i*2 );
                waddch( m_win, ACS_VLINE );
            }
            len -= 2;
        }
        wmove( m_win, line, 31 + i*2 );
        if( treecnt-1 == childline )
        {
            wattroff( m_win, COLOR_PAIR(5) );
            wattron( m_win, COLOR_PAIR(4) );
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
        if( !hilite )
        {
            wattroff( m_win, COLOR_PAIR(5) );
            wattroff( m_win, COLOR_PAIR(4) );
        }
    }
    if( len > 0 )
    {
        auto target = len;
        if( !hilite && wasVisited ) wattron( m_win, COLOR_PAIR(8) | A_BOLD );
        if( SameSubject( subject, prev ) )
        {
            len = 1;
            waddch( m_win, '>' );
        }
        else
        {
            end = utfendl( subject, len );
            utfprint( m_win, subject, end );
        }
        if( !hilite && wasVisited ) wattroff( m_win, COLOR_PAIR(8) | A_BOLD );
    }
    wmove( m_win, line, w-dlen-2 );
    waddch( m_win, '[' );
    if( !hilite ) wattron( m_win, COLOR_PAIR(2) );
    wprintw( m_win, "%s", buf );
    if( !hilite ) wattroff( m_win, COLOR_PAIR(2) );
    waddch( m_win, (m_cursor == idx) ? '<' : ']' );
    if( hilite ) wattroff( m_win, COLOR_PAIR(2) | A_BOLD );

    return m_cursor == idx;
}

void ThreadView::MoveCursor( int offset )
{
    auto end = m_archive.NumberOfMessages();

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
    while( m_cursor != m_archive.NumberOfMessages() && CheckVisited( m_cursor ) );
    if( m_cursor == m_archive.NumberOfMessages() )
    {
        m_cursor = GetPrev( m_cursor );
    }
    FocusOn( m_cursor );
    Draw();
}

int ThreadView::GetNext( int idx )
{
    assert( idx < m_archive.NumberOfMessages() );
    if( m_data[idx].expanded )
    {
        idx++;
    }
    else
    {
        idx += m_archive.GetTotalChildrenCount( idx );
    }
    return idx;
}

int ThreadView::GetPrev( int idx ) const
{
    assert( idx > 0 );
    idx--;
    auto parent = m_archive.GetParent( idx );
    while( parent != -1 )
    {
        if( !m_data[parent].expanded )
        {
            idx = parent;
        }
        parent = m_archive.GetParent( parent );
    }
    return idx;
}

bool ThreadView::CheckVisited( int idx )
{
    if( m_data[idx].visited ) return true;
    auto ret = m_storage.WasVisited( m_archive.GetMessageId( idx ) );
    if( ret ) m_data[idx].visited = true;
    return ret;
}
