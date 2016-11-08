#include <curses.h>
#include "Browser.hpp"

Browser::Browser( std::unique_ptr<Archive>&& archive )
    : m_archive( std::move( archive ) )
    , m_header( m_archive->GetArchiveName().first, m_archive->GetShortDescription().second > 0 ? m_archive->GetShortDescription().first : nullptr )
    , m_bottom( m_archive->NumberOfTopLevel() )
    , m_tview( *m_archive )
{
    m_bottom.Update( 0 );
    doupdate();
}

Browser::~Browser()
{
}

void Browser::Entry()
{
    while( auto key = m_tview.GetKey() )
    {
        switch( key )
        {
        case KEY_RESIZE:
            break;
        case 'q':
            return;
        default:
            break;
        }
    }
}
