#ifndef __THREADVIEW_HPP__
#define __THREADVIEW_HPP__

#include <stdint.h>
#include <vector>

#include "BitSet.hpp"
#include "View.hpp"

class Archive;
class Galaxy;
class MessageView;
class PersistentStorage;

enum { CondensedDepthThreshold = 20 };
enum { CondensedBits = 4 };
enum { CondensedMax = ( 1 << CondensedBits ) - 1 };
enum { CondensedStep = 4 };

struct ThreadData
{
    uint8_t expanded   : 1;
    uint8_t valid      : 1;
    uint8_t visited    : 1;
    uint8_t visall     : 1;
    uint8_t condensed  : CondensedBits;
    uint8_t galaxy     : 3;
};

static_assert( sizeof( ThreadData ) == sizeof( uint16_t ), "Thread data size greater than 2 byte." );

enum class ScoreState
{
    Unknown,
    Neutral,
    Positive,
    Negative
};

class ThreadView : public View
{
public:
    ThreadView( const Archive& archive, PersistentStorage& storage, const Galaxy* galaxy, const MessageView& mview );

    void Reset( const Archive& archive );

    void Resize();
    void Draw();

    void MoveCursor( int offset );
    void GoNextUnread();
    bool CanExpand( int cursor );
    int Expand( int cursor, bool recursive );
    void Collapse( int cursor );
    bool IsExpanded( int cursor ) const { return m_data[cursor].expanded; }
    void FocusOn( int cursor );
    void MarkTreeCondensed( int cursor, int depth );

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
    ScoreState GetScoreState( int idx );

    const Archive* m_archive;
    PersistentStorage& m_storage;
    const Galaxy* m_galaxy;

    const MessageView& m_mview;
    std::vector<ThreadData> m_data;
    std::vector<BitSet> m_tree;
    int m_top, m_bottom;
    int m_cursor;
    int m_fillPos, m_topLevelPos;
};

#endif
