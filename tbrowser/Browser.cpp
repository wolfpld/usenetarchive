#include <curses.h>
#include "Browser.hpp"

Browser::Browser( std::unique_ptr<Archive>&& archive )
    : m_archive( std::move( archive ) )
    , m_header( m_archive->GetArchiveName(), m_archive->GetShortDescription().second > 0 ? m_archive->GetShortDescription().first : nullptr )
    , m_bottom()
    , m_mview( *m_archive )
    , m_tview( *m_archive, m_mview )
{
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
            m_mview.Resize();
            m_tview.Resize();
            m_bottom.Resize();
            doupdate();
            break;
        case 'q':
            if( !m_mview.IsActive() ) return;
            m_mview.Close();
            m_tview.Resize();
            doupdate();
            break;
        case KEY_ENTER:
        case '\n':
        {
            auto resizeNeeded = !m_mview.IsActive();
            m_mview.Display( m_tview.GetMessageIndex(), 1 );
            if( resizeNeeded )
            {
                m_tview.Resize();
                m_mview.Resize();
            }
            doupdate();
            break;
        }
        case KEY_BACKSPACE:
        case '\b':
        {
            auto resizeNeeded = !m_mview.IsActive();
            m_mview.Display( m_tview.GetMessageIndex(), -1 );
            if( resizeNeeded )
            {
                m_tview.Resize();
                m_mview.Resize();
            }
            doupdate();
            break;
        }
        case ' ':
        {
            auto resizeNeeded = !m_mview.IsActive();
            m_mview.Display( m_tview.GetMessageIndex(), m_mview.GetHeight() - 2 );
            if( resizeNeeded )
            {
                m_tview.Resize();
                m_mview.Resize();
            }
            doupdate();
            break;
        }
        case KEY_UP:
        case 'k':
            m_tview.MoveCursor( -1 );
            break;
        case KEY_DOWN:
        case 'j':
            m_tview.MoveCursor( 1 );
            break;
        case KEY_PPAGE:
            m_tview.MoveCursor( -m_tview.GetHeight()+2 );
            break;
        case KEY_NPAGE:
            m_tview.MoveCursor( m_tview.GetHeight()-2 );
            break;
        case 'x':
        {
            auto cursor = m_tview.GetCursor();
            if( m_tview.IsExpanded( cursor ) )
            {
                m_tview.Collapse( cursor );
            }
            else
            {
                m_tview.Expand( cursor, m_archive->GetParent( m_tview.GetMessageIndex() ) == -1 );
            }
            m_tview.Draw();
            doupdate();
            break;
        }
        case KEY_LEFT:
        case 'h':
        {
            auto cursor = m_tview.GetCursor();
            if( m_tview.CanExpand( cursor ) && m_tview.IsExpanded( cursor ) )
            {
                m_tview.Collapse( cursor );
                m_tview.Draw();
                doupdate();
            }
            else
            {
                m_tview.MoveCursor( -1 );
            }
            break;
        }
        case KEY_RIGHT:
        case 'l':
        {
            auto cursor = m_tview.GetCursor();
            if( m_tview.CanExpand( cursor ) && !m_tview.IsExpanded( cursor ) )
            {
                m_tview.Expand( cursor, m_archive->GetParent( m_tview.GetMessageIndex() ) == -1 );
                m_tview.Draw();
                doupdate();
            }
            else
            {
                m_tview.MoveCursor( 1 );
            }
            break;
        }
        default:
            break;
        }
    }
}
