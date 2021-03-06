#ifndef __THREADVIEW_HPP__
#define __THREADVIEW_HPP__

#include "GalaxyState.hpp"
#include "ThreadTree.hpp"
#include "View.hpp"

class Archive;
class Galaxy;
class MessageView;
class PersistentStorage;

class ThreadView : public View
{
public:
    ThreadView( const Archive& archive, PersistentStorage& storage, const Galaxy* galaxy, const MessageView& mview );

    void Reset( const Archive& archive );

    void Resize();
    void Draw();
    void RecalcTopBottom();

    void MoveCursor( int offset );
    void GoNextUnread();
    bool CanExpand( int cursor ) const { return m_tree.CanExpand( cursor ); }
    void Expand( int cursor, bool recursive ) { m_tree.Expand( cursor, recursive ); }
    void Collapse( int cursor ) { m_tree.Collapse( cursor ); }
    bool IsExpanded( int cursor ) const { return m_tree.IsExpanded( cursor ); }
    void FocusOn( int cursor );

    int GetCursor() const { return m_cursor; }
    void SetCursor( int cursor ) { m_cursor = cursor; }
    int GetRoot( int cursor ) const { return m_tree.GetRoot( cursor ); }

    void PageForward();
    void PageBackward();
    void MoveCursorTop();
    void MoveCursorBottom();

    GalaxyState CheckGalaxyState( int cursor ) const { return m_tree.CheckGalaxyState( cursor ); }
    const ThreadTree& GetThreadTree() const { return m_tree; }

private:
    int GetNext( int idx );
    int GetPrev( int idx ) const;

    const Archive* m_archive;

    const MessageView& m_mview;
    ThreadTree m_tree;
    int m_top, m_bottom;
    int m_cursor;
    int m_fillPos, m_topLevelPos;
};

#endif
