#include <algorithm>
#include <assert.h>
#include <chrono>
#include <ctype.h>
#include <sstream>
#include <vector>

#include "../libuat/Galaxy.hpp"
#include "../libuat/PersistentStorage.hpp"

#include "BottomBar.hpp"
#include "Browser.hpp"
#include "GalaxyOpen.hpp"
#include "UTF8.hpp"

GalaxyOpen::GalaxyOpen( Browser* parent, BottomBar& bar, Galaxy& galaxy, PersistentStorage& storage )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_bar( bar )
    , m_storage( storage )
    , m_galaxy( galaxy )
    , m_active( false )
    , m_top( 0 )
    , m_bottom( 0 )
    , m_cursor( 0 )
{
}

void GalaxyOpen::Entry()
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
        case KEY_DOWN:
        case 'j':
            MoveCursor( 1 );
            doupdate();
            break;
        case KEY_UP:
        case 'k':
            MoveCursor( -1 );
            doupdate();
            break;
        case KEY_NPAGE:
            MoveCursor( m_bottom - m_top );
            doupdate();
            break;
        case KEY_PPAGE:
            MoveCursor( m_top - m_bottom );
            doupdate();
            break;
        case KEY_ENTER:
        case '\n':
        case 459:   // numpad enter

            break;
        default:
            break;
        }
        m_bar.Update();
    }
}

void GalaxyOpen::Resize()
{
    ResizeView( 0, 1, 0, -2 );
    if( !m_active ) return;
    Draw();
}

void GalaxyOpen::Draw()
{
    werase( m_win );

    mvwprintw( m_win, 1, 2, "Select archive to open:" );

    wnoutrefresh( m_win );
}

void GalaxyOpen::MoveCursor( int offset )
{
    while( offset < 0 )
    {
        if( m_cursor == 0 ) break;
        m_cursor--;
        if( m_cursor < m_top )
        {
            m_top--;
            m_bottom--;
        }
        offset++;
    }
    while( offset > 0 )
    {
//        if( m_cursor == m_result.results.size() - 1 ) break;
        m_cursor++;
        if( m_cursor >= m_bottom - 1 )
        {
            m_top++;
            m_bottom++;
        }
        offset--;
    }
    Draw();
}
