#include <assert.h>
#include <algorithm>
#include <curses.h>
#include <limits>
#include <time.h>

#include "../common/KillRe.hpp"

#include "../libuat/Archive.hpp"
#include "../libuat/Galaxy.hpp"
#include "../libuat/PersistentStorage.hpp"
#include "../libuat/ViewReference.hpp"

#include "ThreadTree.hpp"
#include "UTF8.hpp"

ThreadTree::ThreadTree( const Archive& archive, PersistentStorage& storage, const Galaxy* galaxy )
    : m_archive( &archive )
    , m_storage( storage )
    , m_galaxy( galaxy )
{
    const auto size = archive.NumberOfMessages();
    m_data = new ThreadData[size];
    m_tree = new BitSet[size];
    memset( m_data, 0, size * sizeof( ThreadData ) );
    memset( m_tree, 0, size * sizeof( BitSet ) );
}

ThreadTree::~ThreadTree()
{
    Cleanup();
}

void ThreadTree::Reset( const Archive& archive )
{
    Cleanup();
    const auto size = archive.NumberOfMessages();
    m_data = new ThreadData[size];
    m_tree = new BitSet[size];
    memset( m_data, 0, size * sizeof( ThreadData ) );
    memset( m_tree, 0, size * sizeof( BitSet ) );
    m_archive = &archive;
}

void ThreadTree::Cleanup()
{
    delete[] m_data;
    delete[] m_tree;
    for( auto& v : m_bitsetCleanup )
    {
        delete v;
    }
    m_bitsetCleanup.clear();
}

GalaxyState ThreadTree::CheckGalaxyState( int idx ) const
{
    assert( m_galaxy );
    auto current = GetGalaxyStateRaw( idx );
    assert( current != GalaxyState::Unknown );
    return current;
}

GalaxyState ThreadTree::GetGalaxyState( int idx )
{
    assert( m_galaxy );
    auto state = GetGalaxyStateRaw( idx );
    if( state == GalaxyState::Unknown )
    {
        const auto msgid = m_archive->GetMessageId( idx );
        const auto gidx  = m_galaxy->GetMessageIndex( msgid );
        const auto groups = m_galaxy->GetNumberOfGroups( gidx );
        assert( groups > 0 );

        ViewReference<uint32_t> ip = {};
        ViewReference<uint32_t> ic = {};
        const auto ind_idx = m_galaxy->GetIndirectIndex( gidx );
        if( ind_idx != -1 )
        {
            ip = m_galaxy->GetIndirectParents( ind_idx );
            ic = m_galaxy->GetIndirectChildren( ind_idx );
        }

        if( groups == 1 && ip.size == 0 && ic.size == 0 )
        {
            state = GalaxyState::Nothing;
        }
        else
        {
            bool parents = !m_galaxy->AreParentsSame( gidx, msgid ) || ip.size != 0;
            bool children = !m_galaxy->AreChildrenSame( gidx, msgid ) || ic.size != 0;
            if( parents )
            {
                if( children )
                {
                    state = GalaxyState::BothDifferent;
                }
                else
                {
                    state = GalaxyState::ParentDifferent;
                }
            }
            else
            {
                if( children )
                {
                    state = GalaxyState::ChildrenDifferent;
                }
                else
                {
                    state = GalaxyState::Crosspost;
                }
            }
        }
        SetGalaxyState( idx, state );
    }
    return state;
}

ScoreState ThreadTree::GetScoreState( int idx )
{
    auto state = GetScoreStateRaw( idx );
    if( state == ScoreState::Unknown )
    {
        int score = m_archive->GetMessageScore( idx, m_storage.GetScoreList() );
        if( score == 0 )
        {
            state = ScoreState::Neutral;
        }
        else if( score < 0 )
        {
            state = ScoreState::Negative;
        }
        else
        {
            state = ScoreState::Positive;
        }
        SetScoreState( idx, state );
    }
    return state;
}

bool ThreadTree::WasVisited( int idx )
{
    if( WasVisitedRaw( idx ) ) return true;
    auto ret = m_storage.WasVisited( m_archive->GetMessageId( idx ) );
    if( ret ) SetVisited( idx, true );
    return ret;
}

bool ThreadTree::WasAllVisited( int idx )
{
    bool complete = WasAllVisitedRaw( idx );
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
            if( !WasVisited( id ) )
            {
                complete = false;
                break;
            }
            stack.pop_back();
            const auto children = m_archive->GetChildren( id );
            for( int i=0; i<children.size; i++ )
            {
                if( !WasAllVisitedRaw( children.ptr[i] ) )
                {
                    stack.emplace_back( children.ptr[i] );
                }
            }
        }
        if( complete )
        {
            SetAllVisited( idx, true );
        }
    }
    return complete;
}

int ThreadTree::GetRoot( int idx ) const
{
    while( m_archive->GetParent( idx ) != -1 ) idx = m_archive->GetParent( idx );
    return idx;
}

bool ThreadTree::CanExpand( int idx ) const
{
    return m_archive->GetTotalChildrenCount( idx ) > 1;
}

void ThreadTree::Expand( int idx, bool recursive )
{
    auto depth = ExpandImpl( idx, recursive );
    if( depth > CondensedDepthThreshold && GetCondensedValue( idx ) == 0 )
    {
        MarkTreeCondensed( idx, depth );
    }
}

void ThreadTree::MarkTreeCondensed( int idx, int depth )
{
    assert( GetRoot( idx ) == idx );
    assert( GetCondensedValue( idx ) == 0 );
    depth = std::min<int>( CondensedMax, ( depth - CondensedDepthThreshold + CondensedStep - 1 ) / CondensedStep );
    assert( depth > 0 );
    auto cnt = m_archive->GetTotalChildrenCount( idx );
    do
    {
        SetCondensedValue( idx++, depth );
    }
    while( --cnt );
}

int ThreadTree::ExpandImpl( int idx, bool recursive )
{
    SetValid( idx, true );
    SetExpanded( idx, true );

    auto children = m_archive->GetChildren( idx );
    int parent = idx;
    idx++;
    int depth = 0;
    for( int i=0; i<children.size; i++ )
    {
        auto skip = m_archive->GetTotalChildrenCount( children.ptr[i] );
        if( !IsValid( idx ) )
        {
            SetValid( idx, true );
            bool line = i != children.size - 1;
            for( int j=0; j<skip; j++ )
            {
                SetTreeLine( idx+j, line );
            }
        }
        if( recursive )
        {
            depth = std::max( depth, ExpandImpl( idx, true ) );
        }
        idx += skip;
    }
    return depth + 1;
}

void ThreadTree::Collapse( int idx )
{
    SetExpanded( idx, false );
}

void ThreadTree::ExpandFill( int idx )
{
    if( idx == m_archive->NumberOfMessages()-1 || IsValid( idx+1 ) ) return;
    auto children = m_archive->GetChildren( idx );
    int parent = idx;
    idx++;
    for( int i=0; i<children.size; i++ )
    {
        auto skip = m_archive->GetTotalChildrenCount( children.ptr[i] );
        assert( !IsValid( idx ) );
        SetValid( idx, true );
        bool line = i != children.size - 1;
        for( int j=0; j<skip; j++ )
        {
            SetTreeLine( idx+j, line );
        }
        ExpandFill( idx );
        idx += skip;
    }
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

void ThreadTree::DrawLine( WINDOW* win, int line, int idx, bool hilite, int colorBase, int cursor, const char*& prev )
{
    bool wasVisited = WasVisited( idx );

    if( hilite ) wattron( win, COLOR_PAIR(2) | A_BOLD );
    if( cursor == idx )
    {
        if( !hilite ) wattron( win, COLOR_PAIR(2) | A_BOLD );
        wmove( win, line, 0 );
        wprintw( win, "->" );
        if( !hilite ) wattroff( win, COLOR_PAIR(2) | A_BOLD );
    }
    else
    {
        wmove( win, line, 2 );
    }

    if( m_galaxy )
    {
        switch( GetGalaxyState( idx ) )
        {
        case GalaxyState::Crosspost:
            if( !hilite ) wattron( win, COLOR_PAIR( 3 ) );
            waddch( win, 'x' );
            if( !hilite ) wattroff( win, COLOR_PAIR( 3 ) );
            break;
        case GalaxyState::ParentDifferent:
            if( !hilite ) wattron( win, COLOR_PAIR( 7 ) );
            waddch( win, 'F' );
            if( !hilite ) wattroff( win, COLOR_PAIR( 7 ) );
            break;
        case GalaxyState::ChildrenDifferent:
            if( !hilite ) wattron( win, COLOR_PAIR( 2 ) );
            waddch( win, '!' );
            if( !hilite ) wattroff( win, COLOR_PAIR( 2 ) );
            break;
        case GalaxyState::BothDifferent:
            if( !hilite ) wattron( win, COLOR_PAIR( 4 ) );
            waddch( win, '&' );
            if( !hilite ) wattroff( win, COLOR_PAIR( 4 ) );
            break;
        default:
            waddch( win, ' ' );
            break;
        }
    }
    if( wasVisited )
    {
        if( WasAllVisited( idx ) )
        {
            waddch( win, 'R' );
        }
        else
        {
            waddch( win, 'r' );
        }
    }
    else
    {
        waddch( win, '-' );
    }

    const auto children = m_archive->GetTotalChildrenCount( idx );
    if( children > 9999 )
    {
        wprintw( win, "++++ [" );
    }
    else
    {
        wprintw( win, "%4i [", children );
    }

    auto realname = m_archive->GetRealName( idx );
    ScoreState ss;
    if( !hilite )
    {
        ss = GetScoreState( idx );
        switch( ss )
        {
        case ScoreState::Neutral:
            wattron( win, COLOR_PAIR(3) | A_BOLD );
            break;
        case ScoreState::Negative:
            wattron( win, COLOR_PAIR(5) );
            break;
        case ScoreState::Positive:
            wattron( win, COLOR_PAIR(7) | A_BOLD );
            break;
        default:
            assert( false );
            break;
        }
    }
    const int lenBase = m_galaxy ? 17 : 18;
    int len = lenBase;
    auto end = utfendl( realname, len );
    utfprint( win, realname, end );
    if( len < lenBase )
    {
        wmove( win, line, 27 );
    }
    if( !hilite )
    {
        switch( ss )
        {
        case ScoreState::Neutral:
            wattroff( win, COLOR_PAIR(3) | A_BOLD );
            break;
        case ScoreState::Negative:
            wattroff( win, COLOR_PAIR(5) );
            break;
        case ScoreState::Positive:
            wattroff( win, COLOR_PAIR(7) | A_BOLD );
            break;
        default:
            assert( false );
            break;
        }
    }
    waddch( win, ']' );

    if( children > 1 )
    {
        if( !hilite ) wattron( win, COLOR_PAIR(4) );
        wmove( win, line, 29 );
        waddch( win, IsExpanded( idx ) ? '-' : '+' );
        if( !hilite ) wattroff( win, COLOR_PAIR(4) );
    }

    time_t date = m_archive->GetDate( idx );
    auto lt = localtime( &date );
    char buf[64];
    auto dlen = strftime( buf, 64, "%F %R", lt );

    wmove( win, line, 31 );
    auto w = getmaxx( win );
    auto subject = m_archive->GetSubject( idx );
    auto treecnt = GetTreeLineSize( idx );
    len = w - 33 - dlen;
    if( treecnt > 0 )
    {
        auto cval = GetCondensedValue( idx );
        const bool condensed = cval == CondensedMax || ( cval * CondensedStep + CondensedDepthThreshold ) * 2 > len;
        int childline = std::numeric_limits<int>::max();
        if( colorBase != -1 && idx > colorBase && idx < colorBase + m_archive->GetTotalChildrenCount( colorBase ) )
        {
            childline = GetTreeLineSize( colorBase );
        }

        const int lw = condensed ? 1 : 2;
        len -= lw;
        if( !hilite ) wattron( win, COLOR_PAIR(5) );
        int i;
        for( i=0; i<treecnt-1 && len > 1; i++ )
        {
            if( childline == i )
            {
                wattroff( win, COLOR_PAIR(5) );
                wattron( win, COLOR_PAIR(4) );
            }
            if( GetTreeLine( idx, i ) )
            {
                wmove( win, line, 31 + i*lw );
                waddch( win, ACS_VLINE );
            }
            len -= lw;
        }
        wmove( win, line, 31 + i*lw );
        if( treecnt-1 == childline )
        {
            wattroff( win, COLOR_PAIR(5) );
            wattron( win, COLOR_PAIR(4) );
        }
        if( GetTreeLine( idx, treecnt-1 ) )
        {
            waddch( win, ACS_LTEE );
        }
        else
        {
            waddch( win, ACS_LLCORNER );
        }
        if( children > 1 && ( condensed || !IsExpanded( idx ) ) )
        {
            waddch( win, ACS_TTEE );
        }
        else
        {
            waddch( win, ACS_HLINE );
        }
        if( !hilite )
        {
            wattroff( win, COLOR_PAIR(5) );
            wattroff( win, COLOR_PAIR(4) );
        }
    }
    if( len > 0 )
    {
        auto target = len;
        if( !hilite && wasVisited ) wattron( win, COLOR_PAIR(8) | A_BOLD );
        if( SameSubject( subject, prev ) )
        {
            len = 1;
            waddch( win, '>' );
        }
        else
        {
            end = utfendl( subject, len );
            utfprint( win, subject, end );
        }
        if( !hilite && wasVisited ) wattroff( win, COLOR_PAIR(8) | A_BOLD );
    }
    wmove( win, line, w-dlen-2 );
    waddch( win, '[' );
    if( !hilite ) wattron( win, COLOR_PAIR(2) );
    wprintw( win, "%s", buf );
    if( !hilite ) wattroff( win, COLOR_PAIR(2) );
    waddch( win, (cursor == idx) ? '<' : ']' );
    if( hilite ) wattroff( win, COLOR_PAIR(2) | A_BOLD );
}

void ThreadTree::SetTreeLine( int idx, bool line )
{
    if( !m_tree[idx].Set( line ) )
    {
        m_bitsetCleanup.emplace_back( m_tree[idx].Convert() );
        auto success = m_tree[idx].Set( line );
        assert( success );
    }
}
