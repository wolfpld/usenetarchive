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
    uint32_t msgid      : 29;
    int32_t parent;
};

static_assert( sizeof( ThreadData ) == sizeof( uint64_t ), "Thread data size greater than 8 bytes." );

class ThreadView : public View
{
public:
    ThreadView( const Archive& archive, PersistentStorage& storage, const MessageView& mview );
    ~ThreadView();

    void Resize();
    void Draw();

    void MoveCursor( int offset );
    bool CanExpand( int cursor );
    void Expand( int cursor, bool recursive );
    void Collapse( int cursor );
    bool IsExpanded( int cursor ) const { return m_data[cursor].expanded; }
    void ScrollTo( int cursor );

    int GetCursor() const { return m_cursor; }
    int GetMessageIndex() const { return m_data[m_cursor].msgid; }

private:
    void Fill( int index, int msgid, int parent );
    void DrawLine( int line, int idx, const char*& prev );

    int GetNext( int idx ) const;
    int GetPrev( int idx ) const;

    bool CheckVisited( int idx );

    const Archive& m_archive;
    PersistentStorage& m_storage;

    const MessageView& m_mview;
    std::vector<ThreadData> m_data;
    std::vector<BitSet> m_tree;
    int m_top, m_bottom;
    int m_cursor;
};

#endif
