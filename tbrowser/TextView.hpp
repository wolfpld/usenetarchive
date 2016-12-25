#ifndef __TEXTVIEW_HPP__
#define __TEXTVIEW_HPP__

#include <string>
#include <vector>

#include "View.hpp"

class Browser;

class TextView : public View
{
public:
    TextView( Browser* parent );

    void Entry( const char* text, int size = -1 );

    void Resize();
    void Draw();

private:
    enum { OffsetBits = 18 };
    enum { LenBits = 10 };
    struct Line
    {
        uint32_t offset     : OffsetBits;
        uint32_t len        : LenBits;
        uint32_t linebreak  : 1;
    };

    void Move( int move );
    void PrepareLines();
    void BreakLine( uint32_t offset, uint32_t len );

    Browser* m_parent;
    bool m_active;

    const char* m_text;
    int m_size;
    std::vector<Line> m_lines;
    int m_linesWidth;
    int m_top;

    static_assert( sizeof( Line ) == sizeof( uint32_t ), "Size of Line greater than 4 bytes." );
};

#endif
