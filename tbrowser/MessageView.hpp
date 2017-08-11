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
    enum Flags
    {
        L_HeaderName,
        L_HeaderBody,
        L_Quote0,
        L_Quote1,
        L_Quote2,
        L_Quote3,
        L_Quote4,
        L_Quote5,
        L_Signature,
        L_LAST
    };

    enum DecoType
    {
        D_None,
        D_Underline,
        D_Italics,
        D_Bold,
        D_LAST
    };

    enum class LineType
    {
        Header,
        Body,
        Signature
    };

    enum { OffsetBits = 28 };
    enum { LenBits = 29 };
    enum { FlagsBits = 4 };
    enum { DecoBits = 2 };
    struct LinePart
    {
        uint64_t offset     : OffsetBits;
        uint64_t len        : LenBits;
        uint64_t flags      : FlagsBits;
        uint64_t deco       : DecoBits;
        uint64_t linebreak  : 1;
    };
    struct Line
    {
        uint32_t idx        : 21;
        uint32_t parts      : 10;
        uint32_t empty      : 1;
    };

    void PrepareLines();
    void AddEmptyLine();
    void BreakLine( uint32_t offset, uint32_t len, LineType type );
    void PrintRot13( const char* start, const char* end );

    void SplitHeader( uint32_t offset, uint32_t len, std::vector<LinePart>& parts );
    void SplitBody( uint32_t offset, uint32_t len, std::vector<LinePart>& parts );
    void Decorate( const char* begin, const char* end, uint64_t flags, std::vector<LinePart>& parts );

    ExpandingBuffer m_eb;
    std::vector<LinePart> m_lineParts;
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

    static_assert( sizeof( LinePart ) == sizeof( uint64_t ), "Size of LinePart greater than 8 bytes." );
    static_assert( sizeof( Line ) == sizeof( uint32_t ), "Size of Line greater than 4 bytes." );
    static_assert( ( 1 << FlagsBits ) >= L_LAST, "Not enough bits for all flags." );
    static_assert( ( 1 << DecoBits ) >= D_LAST, "Not enough bits for all decorations." );
};

#endif
