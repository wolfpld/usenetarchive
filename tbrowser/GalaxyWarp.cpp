#include <algorithm>
#include <assert.h>
#include <chrono>
#include <ctype.h>
#include <sstream>
#include <string.h>
#include <vector>

#include "../common/UTF8.hpp"
#include "../libuat/Galaxy.hpp"
#include "../libuat/PersistentStorage.hpp"

#include "BottomBar.hpp"
#include "Browser.hpp"
#include "GalaxyWarp.hpp"
#include "ThreadTree.hpp"

GalaxyWarp::GalaxyWarp( Browser* parent, BottomBar& bar, Galaxy& galaxy, PersistentStorage& storage )
    : View( 0, 1, 0, -2 )
    , m_parent( parent )
    , m_bar( bar )
    , m_galaxy( galaxy )
    , m_storage( storage )
    , m_active( false )
    , m_cursor( galaxy.GetActiveArchive() )
    , m_top( std::max( 0, m_cursor - 2 ) )
    , m_bottom( 0 )
    , m_msgid( nullptr )
{
}

void GalaxyWarp::Entry( const uint8_t* glxid, GalaxyState state, bool showIndirect, const ThreadTree& currentTree )
{
    char unpack[2048];
    m_galaxy.UnpackMsgId( glxid, unpack );
    m_msgid = unpack;
    m_top = m_bottom = m_cursor = 0;

    bool needCurrent = false;

    assert( m_list.empty() );
    const auto gidx = m_galaxy.GetMessageIndex( glxid );
    const auto groups = m_galaxy.GetGroups( gidx );
    const auto current = m_galaxy.GetActiveArchive();
    for( int i=0; i<groups.size; i++ )
    {
        const auto idx = groups.ptr[i];
        if( m_galaxy.IsArchiveAvailable( idx ) )
        {
            uint8_t local[2048];
            m_galaxy.GetArchive( idx, false )->RepackMsgId( glxid, local, m_galaxy.GetCompress() );

            m_list.emplace_back( WarpEntry { idx, true, current == idx, false, m_msgid,
                m_galaxy.ParentDepth( local, idx ),
                m_galaxy.NumberOfChildren( local, idx ),
                m_galaxy.TotalNumberOfChildren( local, idx ) - 1 } );
        }
        else
        {
            m_list.emplace_back( WarpEntry { idx, false, current == idx, false } );
        }
        if( current == idx )
        {
            m_cursor = i;
            needCurrent = true;
        }
        m_preview.emplace_back( PreviewEntry {} );
    }

    if( showIndirect )
    {
        const auto ind_idx = m_galaxy.GetIndirectIndex( gidx );
        if( ind_idx != -1 )
        {
            const auto ip = m_galaxy.GetIndirectParents( ind_idx );
            for( int i=0; i<ip.size; i++ )
            {
                const auto imsgid = m_galaxy.GetMessageId( ip.ptr[i] );
                const auto igroups = m_galaxy.GetGroups( ip.ptr[i] );

                char unpack[2048];
                m_galaxy.UnpackMsgId( imsgid, unpack );

                for( int j=0; j<igroups.size; j++ )
                {
                    const auto idx = igroups.ptr[j];
                    if( m_galaxy.IsArchiveAvailable( idx ) )
                    {
                        uint8_t local[2048];
                        m_galaxy.GetArchive( idx, false )->RepackMsgId( imsgid, local, m_galaxy.GetCompress() );

                        m_list.emplace_back( WarpEntry { idx, true, false, true, strdup( unpack ),
                            m_galaxy.ParentDepth( local, idx ),
                            m_galaxy.NumberOfChildren( local, idx ),
                            m_galaxy.TotalNumberOfChildren( local, idx ) - 1 } );
                    }
                    else
                    {
                        m_list.emplace_back( WarpEntry { idx, false, false, true } );
                    }
                    if( current == idx ) needCurrent = true;
                    m_preview.emplace_back( PreviewEntry {} );
                }
            }

            const auto ic = m_galaxy.GetIndirectChildren( ind_idx );
            for( int i=0; i<ic.size; i++ )
            {
                const auto imsgid = m_galaxy.GetMessageId( ic.ptr[i] );
                const auto igroups = m_galaxy.GetGroups( ic.ptr[i] );

                char unpack[2048];
                m_galaxy.UnpackMsgId( imsgid, unpack );

                for( int j=0; j<igroups.size; j++ )
                {
                    const auto idx = igroups.ptr[j];
                    if( m_galaxy.IsArchiveAvailable( idx ) )
                    {
                        uint8_t local[2048];
                        m_galaxy.GetArchive( idx, false )->RepackMsgId( imsgid, local, m_galaxy.GetCompress() );

                        m_list.emplace_back( WarpEntry { idx, true, false, true, strdup( unpack ),
                            m_galaxy.ParentDepth( local, idx ),
                            m_galaxy.NumberOfChildren( local, idx ),
                            m_galaxy.TotalNumberOfChildren( local, idx ) - 1 } );
                    }
                    else
                    {
                        m_list.emplace_back( WarpEntry { idx, false, false, true } );
                    }
                    if( current == idx ) needCurrent = true;
                    m_preview.emplace_back( PreviewEntry {} );
                }
            }
        }
    }

    if( needCurrent )
    {
        assert( m_treeCache.empty() );
        m_treeCacheMap.emplace( current, 0 );
        m_treeCache.emplace_back( std::make_unique<ThreadTree>( currentTree ) );
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
            Cleanup();
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
                uint8_t pack[2048];
                archive->PackMsgId( m_list[m_cursor].msgid, pack );
                m_parent->SwitchArchive( archive, m_galaxy.GetArchiveFilename( m_list[m_cursor].id ) );
                m_parent->SwitchToMessage( archive->GetMessageIndex( pack ) );
                m_active = false;
                Cleanup();
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

void GalaxyWarp::Cleanup()
{
    for( auto& v : m_list )
    {
        if( v.indirect )
        {
            free( (void*)v.msgid );
        }
    }
    m_list.clear();
    m_preview.clear();
    m_treeCache.clear();
    m_treeCacheMap.clear();
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
    h = h - 4;  // header
    const int psize = (h+1) / 2;
    h = h - psize;

    werase( m_win );
    mvwprintw( m_win, 1, 2, "Message ID: " );
    wattron( m_win, COLOR_PAIR( 7 ) );
    wprintw( m_win, "%s", m_msgid );
    wattroff( m_win, COLOR_PAIR( 7 ) );
    mvwprintw( m_win, 2, 2, "Select archive to open: (%i/%i)", m_cursor+1, num );

    const int lenBase = w-2;
    int cursorLine = -1;

    int line = m_top;
    for( int i=0; i<(h-4)/2; i++ )
    {
        if( line >= num ) break;

        wattron( m_win, A_BOLD );
        if( m_cursor == line )
        {
            wmove( m_win, 4+i*2, 0 );
            wattron( m_win, COLOR_PAIR( 2 ) );
            wprintw( m_win, "->" );
            cursorLine = i;
        }
        else
        {
            wmove( m_win, 4+i*2, 2 );
        }

        auto name = m_galaxy.GetArchiveName( m_list[line].id );
        auto desc = m_galaxy.GetArchiveDescription( m_list[line].id );
        const bool available = m_list[line].available;
        const bool current = m_list[line].current;

        int len = lenBase;
        auto end = utfendl( name, len );
        wattroff( m_win, COLOR_PAIR( 2 ) );
        if( current ) wattron( m_win, COLOR_PAIR( 4 ) );
        if( !available ) wattron( m_win, COLOR_PAIR( 5 ) );
        wprintw( m_win, "%.*s", int( end - name ), name );

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
            mvwprintw( m_win, 4+i*2, w-len-1, "%.*s", int( end - desc ), desc );
        }
        if( !available ) wattroff( m_win, COLOR_PAIR( 5 ) );
        if( m_cursor == line )
        {
            wattron( m_win, COLOR_PAIR( 2 ) );
            waddch( m_win, '<' );
        }

        wmove( m_win, 5+i*2, 6 );
        wattroff( m_win, A_BOLD );
        if( !available )
        {
            wattron( m_win, COLOR_PAIR( 5 ) );
            wprintw( m_win, "-- Archive not available --" );
            wattroff( m_win, COLOR_PAIR( 5 ) );
        }
        else
        {
            const bool indirect = m_list[line].indirect;
            wattron( m_win, COLOR_PAIR( 3 ) );
            if( m_list[line].parent > 0 )
            {
                wprintw( m_win, "%s at depth %i", indirect ? "Indirect parent" : "Reply", m_list[line].parent );
            }
            else
            {
                wprintw( m_win, "%s of thread", indirect ? "Indirect continuation" : "Start" );
            }
            if( m_list[line].children == 0 )
            {
                wprintw( m_win, ", end of discussion." );
            }
            else
            {
                wprintw( m_win, ", with %i direct repl%s and %i repl%s total.",
                    m_list[line].children, m_list[line].children == 1 ? "y" : "ies",
                    m_list[line].totalchildren, m_list[line].totalchildren == 1 ? "y" : "ies" );
            }
            wattroff( m_win, COLOR_PAIR( 3 ) );
            if( indirect )
            {
                int x, y;
                getyx( m_win, y, x );

                wattron( m_win, COLOR_PAIR( 7 ) );
                if( strlen( m_list[line].msgid ) > w-x-3 )
                {
                    wprintw( m_win, " (%.*s...)", w-x-6, m_list[line].msgid );
                }
                else
                {
                    wprintw( m_win, " (%s)", m_list[line].msgid );
                }
                wattroff( m_win, COLOR_PAIR( 7 ) );
            }
        }

        line++;
    }
    wattroff( m_win, COLOR_PAIR( 2 ) );

    m_bottom = line;

    DrawPreview( psize );

    if( cursorLine != -1 )
    {
        wmove( m_win, 4+cursorLine*2, 1 );
    }

    wnoutrefresh( m_win );
}

void GalaxyWarp::DrawPreview( int size )
{
    int w, h;
    getmaxyx( m_win, h, w );

    int line = h - size;
    wmove( m_win, line, 0 );
    wattron( m_win, COLOR_PAIR( 15 ) | A_BOLD );
    wprintw( m_win, " Thread preview" );
    for( int i=0; i<w-15; i++ )
    {
        waddch( m_win, ' ' );
    }
    wattroff( m_win, COLOR_PAIR( 15 ) | A_BOLD );
    size--;

    if( !m_list[m_cursor].available ) return;
    if( m_preview[m_cursor].end == 0 )
    {
        PreparePreview( m_cursor );
    }

    auto idx = m_preview[m_cursor].idx;
    auto begin = m_preview[m_cursor].begin;
    auto end = m_preview[m_cursor].end;
    auto tree = m_treeCache[m_preview[m_cursor].treeid].get();

    const int mid = size / 2;
    int start = idx;
    while( start > begin && idx - start < mid )
    {
        start--;
    }
    const auto stop = std::min( start + std::min<int>( size, end - begin ), end );

    const char* prev = nullptr;
    for( int i=start; i<stop; i++ )
    {
        tree->DrawLine( m_win, ++line, i, i == idx, idx, idx, prev );
    }
}

void GalaxyWarp::PreparePreview( int cursor )
{
    const auto aid = m_list[cursor].id;
    auto& archive = m_galaxy.GetArchive( aid, false );
    uint8_t pack[2048];
    archive->PackMsgId( m_list[cursor].msgid, pack );
    auto idx = archive->GetMessageIndex( pack );

    int treeid;
    ThreadTree* tree;
    auto it = m_treeCacheMap.find( aid );
    if( it == m_treeCacheMap.end() )
    {
        treeid = m_treeCache.size();
        m_treeCacheMap.emplace( aid, treeid );
        m_treeCache.emplace_back( std::make_unique<ThreadTree>( *archive, m_storage, &m_galaxy ) );
    }
    else
    {
        treeid = it->second;
    }
    tree = m_treeCache[treeid].get();

    auto begin = tree->GetRoot( idx );
    auto end = begin + archive->GetTotalChildrenCount( begin );

    if( tree->CanExpand( begin ) )
    {
        tree->Expand( begin, true );
    }

    m_preview[cursor] = PreviewEntry { treeid, begin, (int)end, idx };
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
