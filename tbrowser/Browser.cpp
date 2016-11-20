#include <curses.h>
#include "../libuat/Archive.hpp"
#include "../libuat/PersistentStorage.hpp"
#include "Browser.hpp"

Browser::Browser( std::unique_ptr<Archive>&& archive, PersistentStorage& storage, const char* fn )
    : m_archive( std::move( archive ) )
    , m_storage( storage )
    , m_header( m_archive->GetArchiveName(), m_archive->GetShortDescription().second > 0 ? m_archive->GetShortDescription().first : nullptr )
    , m_bottom()
    , m_mview( *m_archive, m_storage )
    , m_tview( *m_archive, m_storage, m_mview )
    , m_fn( fn )
{
    auto& history = m_storage.GetArticleHistory();
    if( m_storage.ReadArticleHistory( m_fn ) )
    {
        const auto article = history.back();
        SwitchToMessage( history.back() );
    }
    m_historyIdx = history.size() - 1;

    doupdate();
}

bool Browser::MoveOrEnterAction( int move )
{
    auto resizeNeeded = !m_mview.IsActive();
    bool newMessage = m_mview.DisplayedMessage() != m_tview.GetMessageIndex();
    bool ret = m_mview.Display( m_tview.GetMessageIndex(), move );
    if( resizeNeeded )
    {
        m_tview.Resize();
        m_mview.Resize();
    }
    auto cursor = m_tview.GetCursor();
    if( newMessage )
    {
        if( m_tview.CanExpand( cursor ) &&
            !m_tview.IsExpanded( cursor ) &&
            m_archive->GetParent( m_tview.GetMessageIndex() ) == -1 )
        {
            m_tview.Expand( cursor, true );
        }
        m_tview.Draw();
        m_tview.FocusOn( cursor );

        auto& history = m_storage.GetArticleHistory();
        auto idx = m_tview.GetMessageIndex();
        if( history.empty() || history.back() != idx )
        {
            m_storage.AddToHistory( idx );
            m_historyIdx = history.size()-1;
        }
    }
    return ret;
}

void Browser::Entry()
{
    while( auto key = m_tview.GetKey() )
    {
        switch( key )
        {
        case 12:    // ^L
            endwin();
            initscr();
            // fallthrough
        case KEY_RESIZE:
            resize_term( 0, 0 );
            m_header.Resize();
            m_mview.Resize();
            m_tview.Resize();
            m_bottom.Resize();
            doupdate();
            break;
        case 'e':
        {
            if( m_mview.IsActive() )
            {
                m_mview.Close();
                m_tview.Resize();
            }
            auto idx = m_tview.GetRoot( m_tview.GetCursor() );
            m_tview.SetCursor( idx );
            m_tview.Collapse( idx );
            m_tview.FocusOn( idx );
            doupdate();
            break;
        }
        case 'q':
            if( !m_mview.IsActive() ) return;
            m_mview.Close();
            m_tview.Resize();
            doupdate();
            break;
        case KEY_ENTER:
        case '\n':
        case 459:   // numpad enter
            MoveOrEnterAction( 1 );
            doupdate();
            break;
        case KEY_BACKSPACE:
        case '\b':
        case 127:
            MoveOrEnterAction( -1 );
            doupdate();
            break;
        case ' ':
            if( MoveOrEnterAction( m_mview.GetHeight() - 2 ) )
            {
                m_tview.MoveCursor( 1 );
                MoveOrEnterAction( 0 );
            }
            doupdate();
            break;
        case KEY_DC:
        case 'b':
            MoveOrEnterAction( -m_mview.GetHeight() - 2 );
            doupdate();
            break;
        case 'd':
            m_storage.MarkVisited( m_archive->GetMessageId( m_tview.GetMessageIndex() ) );
            m_tview.GoNextUnread();
            doupdate();
            break;
        case KEY_UP:
        case 'k':
            m_tview.MoveCursor( -1 );
            doupdate();
            break;
        case KEY_DOWN:
        case 'j':
            m_tview.MoveCursor( 1 );
            doupdate();
            break;
        case KEY_PPAGE:
            m_tview.PageBackward();
            m_tview.Draw();
            doupdate();
            break;
        case KEY_NPAGE:
            m_tview.PageForward();
            m_tview.Draw();
            doupdate();
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
                auto parent = m_tview.GetParent( cursor );
                if( parent != -1 )
                {
                    m_tview.SetCursor( parent );
                    m_tview.FocusOn( parent );
                    doupdate();
                }
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
                doupdate();
            }
            break;
        }
        case 't':
            m_mview.SwitchHeaders();
            if( m_mview.IsActive() )
            {
                doupdate();
            }
            break;
        case 'r':
            m_mview.SwitchROT13();
            if( m_mview.IsActive() )
            {
                doupdate();
            }
            break;
        case ',':
        {
            auto& history = m_storage.GetArticleHistory();
            if( !history.empty() && m_historyIdx > 0 )
            {
                m_historyIdx--;
                SwitchToMessage( history[m_historyIdx] );
                doupdate();
            }
            break;
        }
        case '.':
        {
            auto& history = m_storage.GetArticleHistory();
            if( !history.empty() && m_historyIdx < history.size() - 1 )
            {
                m_historyIdx++;
                SwitchToMessage( history[m_historyIdx] );
                doupdate();
            }
            break;
        }
        default:
            break;
        }

        m_bottom.Update();
    }
}

void Browser::SwitchToMessage( int msgidx )
{
    auto cursor = m_tview.ReverseLookup( msgidx );
    if( cursor == -1 )
    {
        auto root = msgidx;
        for(;;)
        {
            auto parent = m_archive->GetParent( root );
            if( parent == -1 ) break;
            root = parent;
        }
        m_tview.Expand( m_tview.ReverseLookupRoot( root ), true );
        m_tview.Draw();
        cursor = m_tview.ReverseLookup( msgidx );
        assert( cursor != -1 );
    }
    else
    {
        auto root = m_tview.GetRoot( cursor );
        if( cursor != root && m_tview.CanExpand( root ) && !m_tview.IsExpanded( root ) )
        {
            m_tview.Expand( root, true );
            m_tview.Draw();
        }
    }
    m_tview.SetCursor( cursor );
    m_tview.FocusOn( cursor );
}
