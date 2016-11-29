#include <curses.h>
#include <locale.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include "../libuat/Archive.hpp"
#include "../libuat/PersistentStorage.hpp"

#include "Browser.hpp"

int main( int argc, char** argv )
{
    std::unique_ptr<Archive> archive;
    PersistentStorage storage;

    auto lastOpen = storage.ReadLastOpenArchive();

    if( argc != 2 )
    {
        if( lastOpen.empty() )
        {
            fprintf( stderr, "Usage: %s archive-name.usenet\n", argv[0] );
            return 1;
        }
    }
    else
    {
        lastOpen = argv[1];
    }

    archive.reset( Archive::Open( lastOpen ) );

    if( !archive )
    {
        fprintf( stderr, "%s is not an archive!\n", argv[1] );
        return 1;
    }

    {
        auto toplevel = archive->GetTopLevel();
        auto children = archive->GetChildren( uint32_t( 0 ) );
        if( !(
            toplevel.ptr[0] == 0 &&
            toplevel.size == 1 ||
            ( children.size == 0 && toplevel.ptr[1] == 1 ) ||
            children.ptr[0] == 1 ) )
        {
            fprintf( stderr, "%s is not a sorted archive!\n", argv[1] );
            return 1;
        }
    }

    setlocale( LC_ALL, "" );

    initscr();
    start_color();
    cbreak();
    noecho();
    keypad( stdscr, TRUE );

    init_pair( 1, COLOR_WHITE, COLOR_BLUE );
    init_pair( 2, COLOR_GREEN, COLOR_BLACK );
    init_pair( 3, COLOR_MAGENTA, COLOR_BLACK );
    init_pair( 4, COLOR_YELLOW, COLOR_BLACK );
    init_pair( 5, COLOR_RED, COLOR_BLACK );
    init_pair( 6, COLOR_BLUE, COLOR_BLACK );
    init_pair( 7, COLOR_CYAN, COLOR_BLACK );
    init_pair( 8, COLOR_BLACK, COLOR_BLACK );
    init_pair( 9, COLOR_CYAN, COLOR_BLUE );
    init_pair( 10, COLOR_RED, COLOR_WHITE );
    init_pair( 11, COLOR_YELLOW, COLOR_BLUE );

    Browser browser( std::move( archive ), storage, lastOpen.c_str() );
    browser.Entry();

    endwin();

    storage.WriteLastOpenArchive( lastOpen.c_str() );
    storage.WriteArticleHistory( lastOpen.c_str() );

    return 0;
}
