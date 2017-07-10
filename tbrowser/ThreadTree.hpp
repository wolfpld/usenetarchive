#ifndef __THREADTREE_HPP__
#define __THREADTREE_HPP__

#include <vector>

#include "BitSet.hpp"
#include "GalaxyState.hpp"
#include "ThreadData.hpp"

class Archive;
class Galaxy;
class PersistentStorage;

class ThreadTree
{
public:
    ThreadTree( const Archive& archive, PersistentStorage& storage, const Galaxy* galaxy, size_t size );
    void Reset( const Archive& archive, size_t size );

    GalaxyState CheckGalaxyState( int idx ) const;
    GalaxyState GetGalaxyState( int idx );
    ScoreState GetScoreState( int idx );
    bool CheckVisited( int idx );
    int GetRoot( int idx ) const;
    bool CanExpand( int idx ) const;
    void Expand( int idx, bool recursive );
    void Collapse( int idx );
    void ExpandFill( int idx );

    bool IsExpanded( int idx ) const { return m_data[idx].expanded; }
    bool WasAllVisited( int idx ) const { return m_data[idx].visall; }
    int GetCondensedValue( int idx ) const { return m_data[idx].condensed; }
    int GetTreeLine( int idx, int line ) const { return m_tree[idx].Get( line ); }
    int GetTreeLineSize( int idx ) const { return m_tree[idx].Size(); }

    void SetAllVisited( int idx, bool val ) { m_data[idx].visall = val; }

private:
    GalaxyState GetGalaxyStateRaw( int idx ) const { return (GalaxyState)m_data[idx].galaxy; }
    ScoreState GetScoreStateRaw( int idx ) const { return (ScoreState)m_tree[idx].GetScoreData(); }
    bool IsValid( int idx ) const { return m_data[idx].valid; }
    bool WasVisited( int idx ) const { return m_data[idx].visited; }

    void SetValid( int idx, bool val ) { m_data[idx].valid = val; }
    void SetExpanded( int idx, bool val ) { m_data[idx].expanded = val; }
    void SetGalaxyState( int idx, GalaxyState state ) { m_data[idx].galaxy = (uint8_t)state; }
    void SetScoreState( int idx, ScoreState state ) { m_tree[idx].SetScoreData( (int)state ); }
    void SetVisited( int idx, bool val ) { m_data[idx].visited = val; }
    void SetCondensedValue( int idx, int val ) { m_data[idx].condensed = val; }
    void SetTreeLine( int idx, bool line ) { m_tree[idx].Set( line ); }

    int ExpandImpl( int idx, bool recursive );
    void MarkTreeCondensed( int idx, int depth );

    std::vector<ThreadData> m_data;
    std::vector<BitSet> m_tree;

    const Archive* m_archive;
    PersistentStorage& m_storage;
    const Galaxy* m_galaxy;
};

#endif
