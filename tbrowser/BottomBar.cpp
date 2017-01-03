#include <stdlib.h>
#include <string.h>

#include "../common/Filesystem.hpp"

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

std::string BottomBar::Query( const char* prompt, const char* entry, bool filesystem )
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
            PrintHelp();
            wrefresh( m_win );
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
        case '\t':
            if( filesystem )
            {
                SuggestFiles( ret, insert );
                break;
            }
            // fallthrough
        default:
            ret.insert( ret.begin() + insert, key );
            insert++;
            break;
        }
    }
}

int BottomBar::KeyQuery( const char* prompt )
{
    const auto len = strlen( prompt );
    for(;;)
    {
        PrintQuery( prompt, "" );
        wmove( m_win, 0, len );
        wrefresh( m_win );

        auto key = GetKey();
        switch( key )
        {
        case KEY_RESIZE:
            m_parent->Resize();
            break;
        default:
            PrintHelp();
            wrefresh( m_win );
            return key;
        }
    }
}

void BottomBar::Status( const char* status, int timeout )
{
    m_reset = timeout;
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
        wprintw( m_win, "x:Co/Ex e:CoAll q:Quit RET:+Ln BCK:-Ln SPC:+Pg Del:-Pg d:MrkRd ,:Bck .:Fwd t:Hdrs r:R13 g:GoTo s:Srch o:Open p:Parn" );
        break;
    case HelpSet::Search:
        waddch( m_win, ACS_DARROW );
        waddch( m_win, ACS_UARROW );
        wprintw( m_win, ":Move q:Quit s:Search RET:Open" );
        break;
    case HelpSet::Text:
        waddch( m_win, ACS_DARROW );
        waddch( m_win, ACS_UARROW );
        wprintw( m_win, ":Move q:Quit" );
        break;
    case HelpSet::GalaxyOpen:
        waddch( m_win, ACS_DARROW );
        waddch( m_win, ACS_UARROW );
        wprintw( m_win, ":Move q:Quit" );
        break;
    default:
        assert( false );
        break;
    };
}

void BottomBar::PrintQuery( const char* prompt, const char* str ) const
{
    werase( m_win );
    wattron( m_win, A_BOLD );
    wprintw( m_win, "%s", prompt );
    wattroff( m_win, A_BOLD );
    wprintw( m_win, "%s", str );
}

void BottomBar::SuggestFiles( std::string& str, int& pos ) const
{
    std::string base;
    std::string file;

    if( Exists( str ) )
    {
        if( IsFile( str ) ) return;
        base = str;
    }
    else
    {
        auto pos = str.rfind( '/' );
        if( pos == std::string::npos )
        {
            pos = str.rfind( '\\' );
            if( pos == std::string::npos ) return;
        }
        base = str.substr( 0, pos );
        if( !Exists( base ) || IsFile( base ) ) return;
        file = str.substr( pos+1 );
    }

    auto list = ListDirectory( base );
    if( list.empty() ) return;

    auto it = list.begin();
    while( it != list.end() )
    {
        if( strncmp( it->c_str(), file.c_str(), file.size() ) == 0 )
        {
            ++it;
        }
        else
        {
            it = list.erase( it );
        }
    }

    if( list.size() == 1 )
    {
        str = base + '/' + list[0];
        pos = str.size();
        return;
    }

    int ms = file.size();
    for(;;)
    {
        auto test = list[0][ms];
        for( int i=1; i<list.size(); i++ )
        {
            if( list[i][ms] != test )
            {
                if( ms != file.size() )
                {
                    str = base + '/' + list[0].substr( 0, ms );
                    pos = str.size();
                }
                return;
            }
        }
        ms++;
    }
}
