#ifndef __MESSAGEVIEW_HPP__
#define __MESSAGEVIEW_HPP__

#include <vector>

#include "View.hpp"

#include "../common/ExpandingBuffer.hpp"

class Archive;
class PersistentStorage;

class MessageView : public View
{
public:
    MessageView( Archive& archive, PersistentStorage& storage );

    void Reset( Archive& archive );

    void Draw();
    void Resize();
    bool Display( uint32_t idx, int move );
    void Close();
    void SwitchHeaders();
    void SwitchROT13();

    bool IsActive() const { return m_active; }
    uint32_t DisplayedMessage() const { return m_idx; }

private:
    enum
    {
        L_Header,
        L_Quote0,
        L_Quote1,
        L_Quote2,
        L_Quote3,
        L_Quote4,
        L_Quote5,
        L_Signature,
        L_LAST
    };

    enum { OffsetBits = 30 };
    enum { LenBits = 30 };
    enum { FlagsBits = 3 };
    struct Line
    {
        uint64_t offset     : OffsetBits;
        uint64_t len        : LenBits;
        uint64_t flags      : FlagsBits;
        uint64_t linebreak  : 1;
    };

    void PrepareLines();
    void BreakLine( uint32_t offset, uint32_t len, uint32_t flags );
    void PrintRot13( const char* start, const char* end );
    void PrintQuotes( const char*& start, int& len, int level );

    ExpandingBuffer m_eb;
    std::vector<Line> m_lines;
    Archive* m_archive;
    PersistentStorage& m_storage;
    const char* m_text;
    int32_t m_idx;
    int m_top;
    int m_linesWidth;
    bool m_active;
    bool m_vertical;
    bool m_allHeaders;
    bool m_rot13;

    static_assert( sizeof( Line ) == sizeof( uint64_t ), "Size of Line greater than 8 bytes." );
    static_assert( ( 1 << FlagsBits ) >= L_LAST, "Not enough bits for all flags." );
};

#endif
