#include <stdlib.h>

#include "BottomBar.hpp"

BottomBar::BottomBar()
    : View( 0, LINES-1, 0, 1 )
    , m_reset( 0 )
{
    PrintHelp();
    wnoutrefresh( m_win );
}

void BottomBar::Update()
{
    if( m_reset > 0 )
    {
        if( --m_reset == 0 )
        {
            PrintHelp();
            wrefresh( m_win );
        }
    }
}

void BottomBar::Resize() const
{
    ResizeView( 0, LINES-1, 0, 1 );
    werase( m_win );
    PrintHelp();
    wnoutrefresh( m_win );
}

std::string BottomBar::Query( const char* prompt )
{
    std::string ret;
    m_reset = 2;

    for(;;)
    {
        PrintQuery( prompt, ret.c_str() );
        auto key = GetKey();
        switch( key )
        {
        case KEY_BACKSPACE:
        case '\b':
        case 127:
            if( !ret.empty() )
            {
                ret.pop_back();
            }
            break;
        case KEY_ENTER:
        case '\n':
        case 459:   // numpad enter
            return ret;
            break;
        case KEY_EXIT:
        case 27:
            return "";
            break;
        default:
            ret.push_back( key );
            break;
        }
    }
}

void BottomBar::Status( const char* status )
{
    m_reset = 2;
    werase( m_win );
    wprintw( m_win, "%s", status );
    wnoutrefresh( m_win );
}

void BottomBar::PrintHelp() const
{
    werase( m_win );
    waddch( m_win, ACS_DARROW );
    waddch( m_win, ACS_UARROW );
    wprintw( m_win, ":Move " );
    waddch( m_win, ACS_RARROW );
    wprintw( m_win, ":Exp " );
    waddch( m_win, ACS_LARROW );
    wprintw( m_win, ":Coll " );
    wprintw( m_win, "x:Co/Ex e:CoAll q:Quit RET:+Ln BCK:-Ln SPC:+Pg d:MrkRd ,:Bck .:Fwd t:Hdrs r:R13 g:GoTo" );
}

void BottomBar::PrintQuery( const char* prompt, const char* str ) const
{
    werase( m_win );
    wattron( m_win, A_BOLD );
    wprintw( m_win, prompt );
    wattroff( m_win, A_BOLD );
    wprintw( m_win, str );
    wrefresh( m_win );
}
