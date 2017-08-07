#include <algorithm>
#include <curses.h>

#include "../libuat/Archive.hpp"
#include "../libuat/Galaxy.hpp"
#include "../libuat/PersistentStorage.hpp"

#include "Browser.hpp"
#include "Help.hpp"
#include "GalaxyOpen.hpp"
#include "GalaxyWarp.hpp"

Browser::Browser( std::shared_ptr<Archive>&& archive, PersistentStorage& storage, Galaxy* galaxy, const std::string& fn )
    : m_archive( std::move( archive ) )
    , m_storage( storage )
    , m_galaxy( galaxy )
    , m_header( m_archive->GetArchiveName(), m_archive->GetShortDescription().second > 0 ? m_archive->GetShortDescription().first : nullptr, fn.c_str(), galaxy != nullptr )
    , m_bottom( this )
    , m_mview( *m_archive, m_storage )
    , m_tview( *m_archive, m_storage, m_galaxy, m_mview )
    , m_sview( this, m_bottom, *m_archive, m_storage )
    , m_textview( this )
    , m_fn( fn )
{
    if( galaxy )
    {
        m_gopen = std::make_unique<GalaxyOpen>( this, m_bottom, *m_galaxy );
        m_gwarp = std::make_unique<GalaxyWarp>( this, m_bottom, *m_galaxy, m_storage );
    }

    auto& history = m_storage.GetArticleHistory();
    if( m_storage.ReadArticleHistory( m_fn.c_str() ) )
    {
        SwitchToMessage( std::min<int>( m_archive->NumberOfMessages() - 1, history.back() ) );
    }
    m_historyIdx = history.size() - 1;

    m_bottom.Status( "Press '?' for help.", 1 );

    doupdate();
}

Browser::~Browser()
{
}

bool Browser::MoveOrEnterAction( int move )
{
    auto resizeNeeded = !m_mview.IsActive();
    bool newMessage = m_mview.DisplayedMessage() != m_tview.GetCursor();
    bool ret = m_mview.Display( m_tview.GetCursor(), move );
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
            m_archive->GetParent( m_tview.GetCursor() ) == -1 )
        {
            m_tview.Expand( cursor, true );
        }
        m_tview.FocusOn( cursor );

        auto& history = m_storage.GetArticleHistory();
        auto idx = m_tview.GetCursor();
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
            Resize();
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
            if( !m_mview.IsActive() )
            {
                const auto key = m_bottom.KeyQuery( "Are you sure you want to quit? [Y/n]" );
                if( key == 'y' || key == 'Y' || key == KEY_ENTER || key == '\n' || key == 459 ) return;
            }
            else
            {
                m_mview.Close();
                m_tview.Resize();
                doupdate();
            }
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
        {
            char unpack[2048];
            m_archive->UnpackMsgId( m_archive->GetMessageId( m_tview.GetCursor() ), unpack );
            m_storage.MarkVisited( unpack );
            m_tview.GoNextUnread();
            doupdate();
            break;
        }
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
        case KEY_HOME:
            m_tview.MoveCursorTop();
            m_tview.Draw();
            doupdate();
            break;
        case KEY_END:
            m_tview.MoveCursorBottom();
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
                m_tview.Expand( cursor, m_archive->GetParent( cursor ) == -1 );
            }
            m_tview.Draw();
            doupdate();
            break;
        }
        case 'p':
        {
            auto parent = m_archive->GetParent( m_tview.GetCursor() );
            if( parent != -1 )
            {
                m_tview.SetCursor( parent );
                m_tview.FocusOn( parent );
                doupdate();
            }
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
                auto parent = m_archive->GetParent( cursor );
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
                m_tview.Expand( cursor, m_archive->GetParent( cursor ) == -1 );
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
        case 'g':
        {
            auto msgid = m_bottom.Query( "MsgID: " );
            if( !msgid.empty() )
            {
                uint8_t pack[2048];
                m_archive->PackMsgId( msgid.c_str(), pack );
                auto idx = m_archive->GetMessageIndex( pack );
                if( idx >= 0 )
                {
                    OpenMessage( idx );
                }
                else
                {
                    if( m_galaxy )
                    {
                        uint8_t gpack[2048];
                        m_galaxy->RepackMsgId( pack, gpack, m_archive->GetCompress() );

                        idx = m_galaxy->GetMessageIndex( gpack );
                        if( idx >= 0 )
                        {
                            auto groups = m_galaxy->GetGroups( idx );
                            if( groups.size == 1 )
                            {
                                if( m_galaxy->IsArchiveAvailable( *groups.ptr ) )
                                {
                                    std::string s = "Go to ";
                                    s += m_galaxy->GetArchiveName( *groups.ptr );
                                    s += "? [Y/n]";
                                    const auto key = m_bottom.KeyQuery( s.c_str() );
                                    if( key == 'y' || key == 'Y' || key == KEY_ENTER || key == '\n' || key == 459 )
                                    {
                                        auto archive = m_galaxy->GetArchive( *groups.ptr );
                                        SwitchArchive( archive, m_galaxy->GetArchiveFilename( *groups.ptr ) );
                                        archive->RepackMsgId( gpack, pack, m_galaxy->GetCompress() );
                                        SwitchToMessage( archive->GetMessageIndex( pack ) );
                                    }
                                }
                                else
                                {
                                    std::string s = m_galaxy->GetArchiveName( *groups.ptr );
                                    s += " is not available.";
                                    m_bottom.Status( s.c_str() );
                                }
                            }
                            else
                            {
                                m_bottom.SetHelp( HelpSet::GalaxyOpen );
                                m_gwarp->Entry( gpack, GalaxyState::Unknown, false, m_tview.GetThreadTree() );
                                m_bottom.SetHelp( HelpSet::Default );
                                RestoreDefaultView();
                            }
                        }
                        else
                        {
                            m_bottom.Status( "No such message." );
                        }
                    }
                    else
                    {
                        m_bottom.Status( "No such message." );
                    }
                }
                doupdate();
            }
            else
            {
                m_bottom.Update();
                doupdate();
            }
            break;
        }
        case '#':
        {
            auto msgid = m_bottom.Query( "Message index: " );
            if( !msgid.empty() )
            {
                auto idx = atoi( msgid.c_str() );
                SwitchToMessage( std::min<int>( std::max( 0, idx ), m_archive->NumberOfMessages() - 1 ) );
                doupdate();
            }
            break;
        }
        case '/':
        case 's':
            m_bottom.SetHelp( HelpSet::Search );
            m_sview.Entry();
            m_bottom.SetHelp( HelpSet::Default );
            RestoreDefaultView();
            doupdate();
            break;
        case '?':
            m_bottom.SetHelp( HelpSet::Text );
            m_textview.Entry( HelpContents );
            m_bottom.SetHelp( HelpSet::Default );
            RestoreDefaultView();
            doupdate();
            break;
        case 'c':
        {
            auto charter = m_archive->GetLongDescription();
            if( charter.second == 0 )
            {
                m_bottom.Status( "Group charter is not available." );
            }
            else
            {
                m_bottom.SetHelp( HelpSet::Text );
                m_textview.Entry( charter.first, charter.second );
                m_bottom.SetHelp( HelpSet::Default );
                RestoreDefaultView();
            }
            doupdate();
            break;
        }
        case 'o':
            if( m_galaxy )
            {
                m_bottom.SetHelp( HelpSet::GalaxyOpen );
                m_gopen->Entry();
                m_bottom.SetHelp( HelpSet::Default );
                RestoreDefaultView();
                doupdate();
            }
            else
            {
                auto fn = m_bottom.Query( "Open archive: ", m_fn.c_str(), true );
                if( !fn.empty() )
                {
                    if( !Exists( fn ) )
                    {
                        m_bottom.Status( "No such archive!" );
                    }
                    else
                    {
                        OpenArchive( std::move( fn ) );
                    }
                }
            }
            break;
        case 'w':
            if( m_galaxy )
            {
                auto state = m_tview.CheckGalaxyState( m_tview.GetCursor() );
                if( state == GalaxyState::Nothing )
                {
                    m_bottom.Status( "Nowhere to warp to." );
                }
                else
                {
                    m_bottom.SetHelp( HelpSet::GalaxyOpen );
                    uint8_t repack[2048];
                    m_galaxy->RepackMsgId( m_archive->GetMessageId( m_tview.GetCursor() ), repack, m_archive->GetCompress() );
                    m_gwarp->Entry( repack, state, true, m_tview.GetThreadTree() );
                    m_bottom.SetHelp( HelpSet::Default );
                    RestoreDefaultView();
                }
            }
            else
            {
                m_bottom.Status( "Galaxy mode is not active." );
            }
            doupdate();
            break;
        default:
            break;
        }

        m_bottom.Update();
    }
}

void Browser::Resize()
{
    resize_term( 0, 0 );
    m_header.Resize();
    m_mview.Resize();
    m_tview.Resize();
    m_sview.Resize();
    m_textview.Resize();
    m_bottom.Resize();
    if( m_galaxy )
    {
        m_gopen->Resize();
        m_gwarp->Resize();
    }
    doupdate();
}

void Browser::OpenMessage( int msgidx )
{
    SwitchToMessage( msgidx );
    MoveOrEnterAction( 0 );
}

void Browser::SwitchToMessage( int msgidx )
{
    auto root = m_tview.GetRoot( msgidx );
    if( msgidx != root && m_tview.CanExpand( root ) && !m_tview.IsExpanded( root ) )
    {
        m_tview.Expand( root, true );
        m_tview.Draw();
    }
    m_tview.SetCursor( msgidx );
    m_tview.FocusOn( msgidx );
}

void Browser::RestoreDefaultView()
{
    m_tview.Draw();
    if( m_mview.IsActive() )
    {
        m_mview.Draw();
    }
}

void Browser::OpenArchive( std::string&& fn )
{
    std::shared_ptr<Archive> archive( Archive::Open( fn ) );
    if( !archive )
    {
        m_bottom.Status( "Cannot open archive!" );
        return;
    }

    SwitchArchive( std::move( archive ), std::move( fn ) );
}

void Browser::SwitchArchive( const std::shared_ptr<Archive>& archive, std::string&& fn )
{
    m_storage.WriteArticleHistory( m_fn.c_str() );

    std::swap( fn, m_fn );
    m_archive = archive;

    m_header.Change( m_archive->GetArchiveName(), m_archive->GetShortDescription().second > 0 ? m_archive->GetShortDescription().first : nullptr, m_fn.c_str() );
    m_mview.Reset( *m_archive );
    m_sview.Reset( *m_archive );
    m_tview.Reset( *m_archive );

    auto& history = m_storage.GetArticleHistory();
    if( m_storage.ReadArticleHistory( m_fn.c_str() ) )
    {
        m_tview.Draw();
        SwitchToMessage( std::min<int>( m_archive->NumberOfMessages() - 1, history.back() ) );
    }
    m_historyIdx = history.size() - 1;

    m_tview.Resize();

    m_bottom.Status( "Archive opened." );

    doupdate();
}
