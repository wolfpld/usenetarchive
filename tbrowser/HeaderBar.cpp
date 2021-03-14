#include <stdlib.h>
#include <string.h>

#include "../common/UTF8.hpp"
#include "HeaderBar.hpp"

HeaderBar::HeaderBar( const std::pair<const char*, uint64_t>& archive, const char* desc, const char* fn, bool galaxy )
    : View( 0, 0, 0, 1 )
    , m_archive( archive.first ? archive.first : fn )
    , m_desc( desc )
    , m_archiveLen( archive.first ? archive.second : strlen( fn ) )
    , m_galaxy( galaxy )
{
    wbkgd( m_win, COLOR_PAIR(1) );
    Redraw();
}

void HeaderBar::Resize() const
{
    ResizeView( 0, 0, 0, 1 );
    werase( m_win );
    Redraw();
}

void HeaderBar::Change( const std::pair<const char*, uint64_t>& archive, const char* desc, const char* fn )
{
    m_archive = archive.first ? archive.first : fn;
    m_desc = desc;
    m_archiveLen = archive.first ? archive.second : strlen( fn );

    werase( m_win );
    Redraw();
}

void HeaderBar::Redraw() const
{
    wattron( m_win, COLOR_PAIR(11) | A_BOLD );
    wprintw( m_win, " Usenet Archive" );
    wattron( m_win, COLOR_PAIR(1) );
    wprintw( m_win, " :: " );
    wattron( m_win, COLOR_PAIR(11) );

    wprintw( m_win, "%.*s", m_archiveLen, m_archive );

    if( m_desc )
    {
        wattron( m_win, COLOR_PAIR(1) );
        wprintw( m_win, " :: " );
        wattron( m_win, COLOR_PAIR(11) );

        int w = getmaxx( m_win ) - 23 - m_archiveLen;
        auto end = utfendcrlf( m_desc, w );
        wprintw( m_win, "%.*s", end - m_desc, m_desc );
    }

    if( m_galaxy )
    {
        wmove( m_win, 0, getmaxx( m_win ) - 8 );
        wattron( m_win, COLOR_PAIR( 14 ) );
        wprintw( m_win, " Galaxy " );
    }

    wnoutrefresh( m_win );
}
