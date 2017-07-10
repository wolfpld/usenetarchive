#include <assert.h>
#include <algorithm>

#include "../libuat/Archive.hpp"
#include "../libuat/Galaxy.hpp"
#include "../libuat/PersistentStorage.hpp"
#include "../libuat/ViewReference.hpp"

#include "ThreadTree.hpp"

ThreadTree::ThreadTree( const Archive& archive, PersistentStorage& storage, const Galaxy* galaxy, size_t size )
    : m_data( size )
    , m_tree( size )
    , m_archive( &archive )
    , m_storage( storage )
    , m_galaxy( galaxy )
{
}

void ThreadTree::Reset( const Archive& archive, size_t size )
{
    m_data = std::vector<ThreadData>( size );
    m_tree = std::vector<BitSet>( size );
    m_archive = &archive;
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

bool ThreadTree::CheckVisited( int idx )
{
    if( WasVisited( idx ) ) return true;
    auto ret = m_storage.WasVisited( m_archive->GetMessageId( idx ) );
    if( ret ) SetVisited( idx, true );
    return ret;
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

void ThreadTree::ExpandFill( int cursor )
{
    if( cursor == m_archive->NumberOfMessages()-1 || IsValid( cursor+1 ) ) return;
    auto children = m_archive->GetChildren( cursor );
    int parent = cursor;
    cursor++;
    for( int i=0; i<children.size; i++ )
    {
        auto skip = m_archive->GetTotalChildrenCount( children.ptr[i] );
        assert( !IsValid( cursor ) );
        SetValid( cursor, true );
        bool line = i != children.size - 1;
        for( int j=0; j<skip; j++ )
        {
            SetTreeLine( cursor+j, line );
        }
        ExpandFill( cursor );
        cursor += skip;
    }
}
