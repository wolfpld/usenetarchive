#include <stdlib.h>
#include <string.h>

#include "BottomBar.hpp"
#include "Browser.hpp"
#include "UTF8.hpp"

BottomBar::BottomBar( Browser* parent )
    : View( 0, LINES-1, 0, 1 )
    , m_parent( parent )
    , m_reset( 0 )
    , m_help( HelpSet::Default )
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

std::string BottomBar::Query( const char* prompt, const char* entry )
{
    std::string ret;
    int insert = 0;
    m_reset = 2;
    int plen = strlen( prompt );

    if( entry && *entry )
    {
        ret = entry;
        insert = ret.size();
    }

    for(;;)
    {
        PrintQuery( prompt, ret.c_str() );
        int slen = utflen_relaxed( ret.c_str(), ret.c_str() + insert );
        wmove( m_win, 0, plen + slen );
        wrefresh( m_win );

        auto key = GetKey();
        switch( key )
        {
        case KEY_BACKSPACE:
        case '\b':
        case 127:
            if( !ret.empty() && insert > 0 )
            {
                int cnt = 1;
                insert--;
                while( iscontinuationbyte( ret[insert] ) )
                {
                    insert--;
                    cnt++;
                }
                ret.erase( insert, cnt );
            }
            break;
        case KEY_DC:
            if( !ret.empty() && insert < ret.size() )
            {
                auto len = codepointlen( ret[insert] );
                ret.erase( insert, len );
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
        case KEY_END:
            insert = ret.size();
            break;
        case KEY_HOME:
            insert = 0;
            break;
        case KEY_LEFT:
            if( insert > 0 )
            {
                insert--;
                while( iscontinuationbyte( ret[insert] ) )
                {
                    insert--;
                }
            }
            break;
        case KEY_RIGHT:
            if( insert < ret.size() ) insert += codepointlen( ret[insert] );
            break;
        case KEY_DOWN:
        case KEY_UP:
            break;
        case KEY_RESIZE:
            m_parent->Resize();
            break;
        default:
            ret.insert( ret.begin() + insert, key );
            insert++;
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

void BottomBar::SetHelp( HelpSet set )
{
    m_help = set;
    PrintHelp();
    wnoutrefresh( m_win );
}

void BottomBar::PrintHelp() const
{
    werase( m_win );
    switch( m_help )
    {
    case HelpSet::Default:
        waddch( m_win, ACS_DARROW );
        waddch( m_win, ACS_UARROW );
        wprintw( m_win, ":Move " );
        waddch( m_win, ACS_RARROW );
        wprintw( m_win, ":Exp " );
        waddch( m_win, ACS_LARROW );
        wprintw( m_win, ":Coll " );
        wprintw( m_win, "x:Co/Ex e:CoAll q:Quit RET:+Ln BCK:-Ln SPC:+Pg d:MrkRd ,:Bck .:Fwd t:Hdrs r:R13 g:GoTo s:Srch" );
        break;
    case HelpSet::Search:
        waddch( m_win, ACS_DARROW );
        waddch( m_win, ACS_UARROW );
        wprintw( m_win, ":Move q:Quit /:Search" );
        break;
    default:
        break;
    };
}

void BottomBar::PrintQuery( const char* prompt, const char* str ) const
{
    werase( m_win );
    wattron( m_win, A_BOLD );
    wprintw( m_win, prompt );
    wattroff( m_win, A_BOLD );
    wprintw( m_win, str );
}
