#ifndef __THREADVIEW_HPP__
#define __THREADVIEW_HPP__

#include <stdint.h>
#include <vector>

#include "BitSet.hpp"
#include "View.hpp"

class Archive;
class MessageView;
class PersistentStorage;

struct ThreadData
{
    uint8_t expanded   : 1;
    uint8_t valid      : 1;
    uint8_t visited    : 1;
    uint8_t visall     : 1;
    uint8_t condensed  : 1;
};

static_assert( sizeof( ThreadData ) == sizeof( uint8_t ), "Thread data size greater than 1 bytes." );

class ThreadView : public View
{
public:
    ThreadView( const Archive& archive, PersistentStorage& storage, const MessageView& mview );

    void Resize();
    void Draw();

    void MoveCursor( int offset );
    void GoNextUnread();
    bool CanExpand( int cursor );
    int Expand( int cursor, bool recursive );
    void Collapse( int cursor );
    bool IsExpanded( int cursor ) const { return m_data[cursor].expanded; }
    void FocusOn( int cursor );
    void MarkTreeCondensed( int cursor );

    int GetCursor() const { return m_cursor; }
    void SetCursor( int cursor ) { m_cursor = cursor; }
    int GetRoot( int cursor ) const;

    void PageForward();
    void PageBackward();
    void MoveCursorTop();
    void MoveCursorBottom();

private:
    void ExpandFill( int cursor );
    bool DrawLine( int line, int idx, const char*& prev );

    int GetNext( int idx );
    int GetPrev( int idx ) const;

    bool CheckVisited( int idx );

    const Archive& m_archive;
    PersistentStorage& m_storage;

    const MessageView& m_mview;
    std::vector<ThreadData> m_data;
    std::vector<BitSet> m_tree;
    int m_top, m_bottom;
    int m_cursor;
    int m_fillPos, m_topLevelPos;
};

#endif
