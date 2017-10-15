#ifndef __THREADTREE_HPP__
#define __THREADTREE_HPP__

#include <curses.h>
#include <vector>

#include "../common/KillRe.hpp"

#include "BitSet.hpp"
#include "GalaxyState.hpp"
#include "ThreadData.hpp"

class Archive;
class Galaxy;
class PersistentStorage;

class ThreadTree
{
public:
    ThreadTree( const Archive& archive, PersistentStorage& storage, const Galaxy* galaxy );
    explicit ThreadTree( const ThreadTree& ref );
    ThreadTree( ThreadTree&& ) = delete;

    ~ThreadTree();
    void Reset( const Archive& archive );
    void Cleanup();

    GalaxyState CheckGalaxyState( int idx ) const;
    bool WasVisited( int idx );
    int GetRoot( int idx ) const;
    bool CanExpand( int idx ) const;
    void Expand( int idx, bool recursive );
    void Collapse( int idx );

    bool IsExpanded( int idx ) const { return m_data[idx].expanded; }

    void DrawLine( WINDOW* win, int line, int idx, bool hilite, int colorBase, int cursor, const char*& prev );

    ThreadTree& operator=( const ThreadTree& ) = delete;
    ThreadTree& operator=( ThreadTree&& ) = delete;

private:
    GalaxyState GetGalaxyState( int idx );
    ScoreState GetScoreState( int idx );
    bool IsValid( int idx ) const { return m_data[idx].valid; }
    bool WasVisitedRaw( int idx ) const { return m_data[idx].visited; }
    bool WasAllVisited( int idx );
    int GetCondensedValue( int idx ) const { return m_data[idx].condensed; }
    int GetTreeLine( int idx, int line ) const { return m_tree[idx].Get( line ); }
    int GetTreeLineSize( int idx ) const { return m_tree[idx].Size(); }

    void SetValid( int idx, bool val ) { m_data[idx].valid = val; }
    void SetExpanded( int idx, bool val ) { m_data[idx].expanded = val; }
    void SetGalaxyState( int idx, GalaxyState state ) { m_data[idx].galaxy = (uint8_t)state; }
    void SetScoreState( int idx, ScoreState state ) { m_tree[idx].SetScoreData( (int)state ); }
    void SetVisited( int idx, bool val ) { m_data[idx].visited = val; }
    void SetAllVisited( int idx, bool val ) { m_data[idx].visall = val; }
    void SetCondensedValue( int idx, int val ) { m_data[idx].condensed = val; }
    void SetTreeLine( int idx, bool line );

    int ExpandImpl( int idx, bool recursive );
    void MarkTreeCondensed( int idx, int depth );

    GalaxyState GetGalaxyStateRaw( int idx ) const { return (GalaxyState)m_data[idx].galaxy; }
    ScoreState GetScoreStateRaw( int idx ) const { return (ScoreState)m_tree[idx].GetScoreData(); }
    bool WasAllVisitedRaw( int idx ) const { return m_data[idx].visall; }

    ThreadData* m_data;
    BitSet* m_tree;
    std::vector<std::vector<bool>*> m_bitsetCleanup;

    KillRe m_killre;

    const Archive* m_archive;
    PersistentStorage& m_storage;
    const Galaxy* m_galaxy;

};

#endif
