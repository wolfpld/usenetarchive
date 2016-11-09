#include <curses.h>
#include "Browser.hpp"

Browser::Browser( std::unique_ptr<Archive>&& archive )
    : m_archive( std::move( archive ) )
    , m_header( m_archive->GetArchiveName().first, m_archive->GetShortDescription().second > 0 ? m_archive->GetShortDescription().first : nullptr )
    , m_bottom( m_archive->NumberOfMessages() )
    , m_tview( *m_archive, m_bottom )
{
    m_bottom.Update( m_tview.GetCursor() + 1 );
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
            resize_term( 0, 0 );
            m_header.Resize();
            m_bottom.Resize( m_tview.GetCursor() + 1 );
            m_tview.Resize();
            doupdate();
            break;
        case 'q':
            return;
        case KEY_UP:
        case 'k':
            m_tview.Up();
            break;
        case KEY_DOWN:
        case 'j':
            m_tview.Down();
            break;
        default:
            break;
        }
    }
}
