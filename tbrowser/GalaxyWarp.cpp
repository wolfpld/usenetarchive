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
#include "GalaxyWarp.hpp"
#include "UTF8.hpp"

GalaxyWarp::GalaxyWarp( Browser* parent, BottomBar& bar, Galaxy& galaxy, PersistentStorage& storage )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_bar( bar )
    , m_storage( storage )
    , m_galaxy( galaxy )
    , m_active( false )
    , m_cursor( galaxy.GetActiveArchive() )
    , m_top( std::max( 0, m_cursor - 2 ) )
    , m_bottom( 0 )
    , m_msgid( nullptr )
{
}

void GalaxyWarp::Entry( const char* msgid, GalaxyState state )
{
    m_msgid = msgid;
    m_top = m_bottom = m_cursor = 0;

    m_list.clear();
    const auto groups = m_galaxy.GetGroups( msgid );
    const auto current = m_galaxy.GetActiveArchive();
    for( int i=0; i<groups.size; i++ )
    {
        const auto idx = groups.ptr[i];
        m_list.emplace_back( WarpEntry { idx, m_galaxy.IsArchiveAvailable( idx ), current == idx } );
        if( current == idx )
        {
            m_cursor = i;
        }
    }

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
            MoveCursor( m_bottom - m_top - 1 );
            doupdate();
            break;
        case KEY_PPAGE:
            MoveCursor( m_top - m_bottom + 1 );
            doupdate();
            break;
        case KEY_ENTER:
        case '\n':
        case 459:   // numpad enter
            if( m_list[m_cursor].available )
            {
                auto archive = m_galaxy.GetArchive( m_list[m_cursor].id );
                m_parent->SwitchArchive( archive, m_galaxy.GetArchiveFilename( m_list[m_cursor].id ) );
                m_parent->SwitchToMessage( archive->GetMessageIndex( msgid ) );
                m_active = false;
                return;
            }
            else
            {
                m_bar.Status( "Archive unavailable!" );
                doupdate();
            }
            break;
        default:
            break;
        }
        m_bar.Update();
    }
}

void GalaxyWarp::Resize()
{
    ResizeView( 0, 1, 0, -2 );
    if( !m_active ) return;
    Draw();
}

void GalaxyWarp::Draw()
{
    assert( m_msgid );

    const int num = m_list.size();
    int w, h;
    getmaxyx( m_win, h, w );

    werase( m_win );
    mvwprintw( m_win, 1, 2, "Message ID: " );
    wattron( m_win, COLOR_PAIR( 7 ) );
    wprintw( m_win, "%s", m_msgid );
    wattroff( m_win, COLOR_PAIR( 7 ) );
    mvwprintw( m_win, 2, 2, "Select archive to open: (%i/%i)", m_cursor+1, num );

    const int lenBase = w-2;

    wattron( m_win, A_BOLD );
    int line = m_top;
    for( int i=0; i<h-4; i++ )
    {
        if( line >= num ) break;

        if( m_cursor == line )
        {
            wmove( m_win, 4+i, 0 );
            wattron( m_win, COLOR_PAIR( 2 ) );
            wprintw( m_win, "->" );
        }
        else
        {
            wmove( m_win, 4+i, 2 );
        }

        auto name = m_galaxy.GetArchiveName( m_list[line].id );
        auto desc = m_galaxy.GetArchiveDescription( m_list[line].id );
        bool available = m_list[line].available;
        bool current = m_list[line].current;

        int len = lenBase;
        auto end = utfendl( name, len );
        wattroff( m_win, COLOR_PAIR( 2 ) );
        if( current ) wattron( m_win, COLOR_PAIR( 4 ) );
        if( !available ) wattron( m_win, COLOR_PAIR( 5 ) );
        wprintw( m_win, "%.*s", end - name, name );

        if( available )
        {
            wattron( m_win, COLOR_PAIR( 8 ) );
            char tmp[64];
            sprintf( tmp, "  {%i msg, %i thr}", m_galaxy.NumberOfMessages( m_list[line].id ), m_galaxy.NumberOfTopLevel( m_list[line].id ) );
            wprintw( m_win, "%s", tmp );
            len += strlen( tmp );
            wattroff( m_win, COLOR_PAIR( 8 ) );
        }

        len = lenBase - len - 2;
        if( len > 0 )
        {
            end = utfendcrlfl( desc, len );
            if( available ) wattron( m_win, COLOR_PAIR( 2 ) );
            mvwprintw( m_win, 4+i, w-len-1, "%.*s", end - desc, desc );
        }
        if( !available ) wattroff( m_win, COLOR_PAIR( 5 ) );
        if( m_cursor == line )
        {
            wattron( m_win, COLOR_PAIR( 2 ) );
            waddch( m_win, '<' );
        }
        line++;
    }
    wattroff( m_win, COLOR_PAIR( 2 ) | A_BOLD );

    m_bottom = line;

    wnoutrefresh( m_win );
}

void GalaxyWarp::MoveCursor( int offset )
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
        if( m_cursor == m_list.size() - 1 ) break;
        m_cursor++;
        if( m_cursor >= m_bottom )
        {
            m_top++;
            m_bottom++;
        }
        offset--;
    }
    Draw();
}
