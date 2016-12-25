#include <algorithm>
#include <assert.h>
#include <chrono>
#include <ctype.h>
#include <sstream>
#include <vector>

#include "Browser.hpp"
#include "TextView.hpp"
#include "UTF8.hpp"

TextView::TextView( Browser* parent )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_active( false )
{
}

void TextView::Entry()
{
    m_active = true;
    Draw();
    doupdate();

    while( auto key = GetKey() )
    {
        switch( key )
        {
        case KEY_RESIZE:
            m_parent->Resize();
            break;
        case 'q':
            m_active = false;
            return;
        default:
            break;
        }
    }
}

void TextView::Resize()
{
    ResizeView( 0, 1, 0, -2 );
    if( !m_active ) return;
    Draw();
}

void TextView::Draw()
{
    werase( m_win );
    wnoutrefresh( m_win );
}
