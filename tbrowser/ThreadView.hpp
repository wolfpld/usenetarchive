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
    uint32_t expanded   : 1;
    uint32_t valid      : 1;
    uint32_t visited    : 1;
    uint32_t visall     : 1;
    uint32_t msgid      : 28;
    int32_t parent;
};

static_assert( sizeof( ThreadData ) == sizeof( uint64_t ), "Thread data size greater than 8 bytes." );

class ThreadView : public View
{
public:
    ThreadView( const Archive& archive, PersistentStorage& storage, const MessageView& mview );

    void Resize();
    void Draw();

    void MoveCursor( int offset );
    void GoNextUnread();
    bool CanExpand( int cursor );
    void Expand( int cursor, bool recursive );
    void Collapse( int cursor );
    bool IsExpanded( int cursor ) const { return m_data[cursor].expanded; }
    void FocusOn( int cursor );

    int GetCursor() const { return m_cursor; }
    void SetCursor( int cursor ) { m_cursor = cursor; }
    int GetMessageIndex() const { return m_data[m_cursor].msgid; }
    int GetRoot( int cursor ) const;
    int GetParent( int cursor ) const;
    int32_t ReverseLookup( int msgidx ) const { return m_revLookup[msgidx]; }
    int32_t ReverseLookupRoot( int msgidx );

    void PageForward();
    void PageBackward();

private:
    void ExpandFill( int cursor );
    void Fill( int index, int msgid, int parent );
    void DrawLine( int line, int idx, const char*& prev );
    void FillTo( int index );
    void FillToMsgIdx( int msgidx );

    int GetNext( int idx );
    int GetPrev( int idx ) const;

    bool CheckVisited( int idx );

    const Archive& m_archive;
    PersistentStorage& m_storage;

    const MessageView& m_mview;
    std::vector<ThreadData> m_data;
    std::vector<int32_t> m_revLookup;
    std::vector<BitSet> m_tree;
    int m_top, m_bottom;
    int m_cursor;
    int m_fillPos, m_topLevelPos;
};

#endif
