#ifndef __THREADVIEW_HPP__
#define __THREADVIEW_HPP__

#include <stdint.h>
#include <vector>

#include "BitSet.hpp"
#include "View.hpp"

class Archive;
class MessageView;

struct ThreadData
{
    unsigned int expanded   : 1;
    unsigned int valid      : 1;
    unsigned int msgid      : 30;
    int parent;
};

class ThreadView : public View
{
public:
    ThreadView( const Archive& archive, const MessageView& mview );
    ~ThreadView();

    void Resize();
    void Draw();

    void MoveCursor( int offset );
    bool CanExpand( int cursor );
    void Expand( int cursor, bool recursive );
    void Collapse( int cursor );
    bool IsExpanded( int cursor ) const { return m_data[cursor].expanded; }

    int GetCursor() const { return m_cursor; }
    int GetMessageIndex() const { return m_data[m_cursor].msgid; }

private:
    void Fill( int index, int msgid, int parent );
    void DrawLine( int line, int idx, const char*& prev ) const;

    int GetNext( int idx ) const;
    int GetPrev( int idx ) const;

    const Archive& m_archive;
    const MessageView& m_mview;
    std::vector<ThreadData> m_data;
    std::vector<BitSet> m_tree;
    int m_top, m_bottom;
    int m_cursor;
};

#endif
