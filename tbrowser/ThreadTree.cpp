#include <assert.h>

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
