#include <assert.h>
#include <stdlib.h>
#include <time.h>

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
    , m_revLookup( archive.NumberOfMessages() )
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

    const char* prev = nullptr;
    auto idx = m_top;
    for( int i=0; i<h-1; i++ )
    {
        assert( m_data[idx].valid == 1 );
        if( m_data[idx].parent == -1 ) prev = nullptr;
        DrawLine( i, idx, prev );
        idx = GetNext( idx );
        if( idx >= m_archive.NumberOfMessages() ) break;
    }
    m_bottom = idx;

    wmove( m_win, h-1, 0 );
    wattron( m_win, COLOR_PAIR( 1 ) );
    for( int i=0; i<w; i++ )
    {
        waddch( m_win, ' ' );
    }
    wmove( m_win, h-1, 0 );
    wprintw( m_win, " [%i/%i]", m_cursor+1, m_archive.NumberOfMessages() );
    wattroff( m_win, COLOR_PAIR( 1 ) );

    wnoutrefresh( m_win );
}

bool ThreadView::CanExpand( int cursor )
{
    assert( m_data[cursor].valid );
    return m_archive.GetTotalChildrenCount( m_data[cursor].msgid ) > 1;
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

int ThreadView::GetRoot( int cursor ) const
{
    assert( m_data[cursor].valid );
    while( m_data[cursor].parent != -1 ) cursor = m_data[cursor].parent;
    return cursor;
}

void ThreadView::ExpandFill( int cursor )
{
    if( cursor == m_archive.NumberOfMessages()-1 || m_data[cursor+1].valid ) return;
    auto children = m_archive.GetChildren( m_data[cursor].msgid );
    int parent = cursor;
    cursor++;
    for( int i=0; i<children.size; i++ )
    {
        auto skip = m_archive.GetTotalChildrenCount( children.ptr[i] );
        assert( !m_data[cursor].valid );
        Fill( cursor, children.ptr[i], parent );
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

void ThreadView::Fill( int index, int msgid, int parent )
{
    assert( m_data[index].valid == 0 );
    m_data[index].msgid = msgid;
    m_data[index].valid = 1;
    m_data[index].parent = parent;
    m_revLookup[msgid] = index;
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

void ThreadView::DrawLine( int line, int idx, const char*& prev )
{
    bool hilite = m_mview.IsActive() && m_mview.DisplayedMessage() == m_data[idx].msgid;
    bool wasVisited = CheckVisited( idx );

    wmove( m_win, line, 0 );
    if( hilite ) wattron( m_win, COLOR_PAIR(2) | A_BOLD );
    const auto midx = m_data[idx].msgid;
    if( m_cursor == idx )
    {
        if( !hilite ) wattron( m_win, COLOR_PAIR(2) | A_BOLD );
        wprintw( m_win, "->" );
        if( !hilite ) wattroff( m_win, COLOR_PAIR(2) | A_BOLD );
    }
    else
    {
        if( wasVisited )
        {
            bool complete = m_data[idx].visall;
            if( !complete )
            {
                ExpandFill( idx );
                complete = true;
                std::vector<uint32_t> stack;
                stack.reserve( 4 * 1024 );
                stack.push_back( m_data[idx].msgid );
                while( !stack.empty() )
                {
                    const auto id = stack.back();
                    auto didx = m_revLookup[id];
                    assert( m_data[didx].valid );
                    if( !CheckVisited( didx ) )
                    {
                        complete = false;
                        break;
                    }
                    stack.pop_back();
                    const auto children = m_archive.GetChildren( id );
                    for( int i=0; i<children.size; i++ )
                    {
                        if( !m_data[m_revLookup[children.ptr[i]]].visall )
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
                wprintw( m_win, " R" );
            }
            else
            {
                wprintw( m_win, " r" );
            }
        }
        else
        {
            wprintw( m_win, " -" );
        }
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
    if( !hilite ) wattron( m_win, COLOR_PAIR(3) | A_BOLD );
    int len = 18;
    auto end = utfendl( realname, len );
    utfprint( m_win, realname, end );
    while( len++ < 18 )
    {
        waddch( m_win, ' ' );
    }
    if( !hilite ) wattroff( m_win, COLOR_PAIR(3) | A_BOLD );
    wprintw( m_win, "] " );

    if( children == 1 )
    {
        wprintw( m_win, "  " );
    }
    else
    {
        if( !hilite ) wattron( m_win, COLOR_PAIR(4) );
        waddch( m_win, m_data[idx].expanded ? '-' : '+' );
        waddch( m_win, ' ' );
        if( !hilite ) wattroff( m_win, COLOR_PAIR(4) );
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
        if( !hilite ) wattron( m_win, COLOR_PAIR(5) );
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
        if( !hilite ) wattroff( m_win, COLOR_PAIR(5) );
    }
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
    while( len++ < target )
    {
        waddch( m_win, ' ' );
    }
    waddch( m_win, '[' );
    if( !hilite ) wattron( m_win, COLOR_PAIR(2) );
    wprintw( m_win, "%s", buf );
    if( !hilite ) wattroff( m_win, COLOR_PAIR(2) );
    waddch( m_win, ']' );
    if( hilite ) wattroff( m_win, COLOR_PAIR(2) | A_BOLD );
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

int ThreadView::GetNext( int idx ) const
{
    assert( idx < m_archive.NumberOfMessages() );
    if( m_data[idx].expanded )
    {
        idx++;
    }
    else
    {
        idx += m_archive.GetTotalChildrenCount( m_data[idx].msgid );
    }
    return idx;
}

int ThreadView::GetPrev( int idx ) const
{
    assert( idx > 0 );
    while( !m_data[--idx].valid );
    auto parent = m_data[idx].parent;
    while( parent != -1 )
    {
        if( !m_data[parent].expanded )
        {
            idx = parent;
        }
        parent = m_data[parent].parent;
    }
    return idx;
}

bool ThreadView::CheckVisited( int idx )
{
    if( m_data[idx].visited ) return true;
    auto ret = m_storage.WasVisited( m_archive.GetMessageId( m_data[idx].msgid ) );
    if( ret ) m_data[idx].visited = true;
    return ret;
}
