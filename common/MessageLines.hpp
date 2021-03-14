#ifndef __MESSAGELINES_HPP__
#define __MESSAGELINES_HPP__

#include <stdint.h>
#include <vector>

class MessageLines
{
public:
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
        uint32_t essential  : 1;
    };

    MessageLines();

    void SetWidth( int width ) { m_width = width; }
    void PrepareLines( const char* text, bool skipHeaders );
    void Reset();

    const std::vector<Line>& Lines() const { return m_lines; }
    const std::vector<LinePart>& Parts() const { return m_lineParts; }

private:
    void AddEmptyLine();
    void BreakLine( uint32_t offset, uint32_t len, LineType type, std::vector<LinePart>& partsTmpBuf, const char* text, bool essential );
    void SplitHeader( uint32_t offset, uint32_t len, std::vector<LinePart>& parts, const char* text );
    void SplitBody( uint32_t offset, uint32_t len, std::vector<LinePart>& parts, const char* text );
    void Decorate( const char* begin, const char* end, uint64_t flags, std::vector<LinePart>& parts, const char* text );

    std::vector<LinePart> m_lineParts;
    std::vector<Line> m_lines;
    std::vector<LinePart> m_tmpParts;
    int m_width;

    static_assert( sizeof( LinePart ) == sizeof( uint64_t ), "Size of LinePart greater than 8 bytes." );
    static_assert( sizeof( Line ) == sizeof( uint32_t ), "Size of Line greater than 4 bytes." );
    static_assert( ( 1 << FlagsBits ) >= L_LAST, "Not enough bits for all flags." );
    static_assert( ( 1 << DecoBits ) >= D_LAST, "Not enough bits for all decorations." );
};

#endif
