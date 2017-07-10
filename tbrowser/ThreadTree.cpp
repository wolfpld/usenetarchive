#include <assert.h>

#include "../libuat/Archive.hpp"
#include "../libuat/Galaxy.hpp"
#include "../libuat/ViewReference.hpp"

#include "ThreadTree.hpp"

ThreadTree::ThreadTree( const Archive& archive, const Galaxy* galaxy, size_t size )
    : m_data( size )
    , m_tree( size )
    , m_archive( &archive )
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
    auto current = GetGalaxyStateRaw( idx );
    if( current != GalaxyState::Unknown ) return current;

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
        SetGalaxyState( idx, GalaxyState::Nothing );
    }
    else
    {
        bool parents = !m_galaxy->AreParentsSame( gidx, msgid ) || ip.size != 0;
        bool children = !m_galaxy->AreChildrenSame( gidx, msgid ) || ic.size != 0;
        if( parents )
        {
            if( children )
            {
                SetGalaxyState( idx, GalaxyState::BothDifferent );
            }
            else
            {
                SetGalaxyState( idx, GalaxyState::ParentDifferent );
            }
        }
        else
        {
            if( children )
            {
                SetGalaxyState( idx, GalaxyState::ChildrenDifferent );
            }
            else
            {
                SetGalaxyState( idx, GalaxyState::Crosspost );
            }
        }
    }

    return GetGalaxyStateRaw( idx );
}
