#include <algorithm>
#include <curses.h>
#include <locale.h>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include "../libuat/Archive.hpp"
#include "../libuat/Galaxy.hpp"
#include "../libuat/PersistentStorage.hpp"

#include "Browser.hpp"

int main( int argc, char** argv )
{
    std::shared_ptr<Archive> archive;
    std::unique_ptr<Galaxy> galaxy;
    PersistentStorage storage;

    auto lastOpen = storage.ReadLastOpenArchive();
    bool lastOpenFromStorage = !lastOpen.empty();

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
        lastOpenFromStorage = false;
    }

    storage.Preload();

    galaxy.reset( Galaxy::Open( lastOpen ) );
    if( galaxy )
    {
        auto available = galaxy->GetAvailableArchives();
        if( available.empty() )
        {
            fprintf( stderr, "No available archives in galaxy!\n" );
            return 1;
        }
        auto galaxyLast = storage.ReadLastOpenGalaxyArchive();
        if( galaxyLast >= galaxy->GetNumberOfArchives() || !galaxy->IsArchiveAvailable( galaxyLast ) )
        {
            galaxyLast = available.front();
        }
        if( !lastOpenFromStorage )
        {
            storage.WriteLastOpenArchive( lastOpen.c_str() );
        }
        lastOpen = galaxy->GetArchiveFilename( galaxyLast );
        archive = galaxy->GetArchive( galaxyLast );
    }
    else
    {
        archive.reset( Archive::Open( lastOpen ) );
    }

    if( !archive )
    {
        fprintf( stderr, "%s is not an archive!\n", lastOpen.c_str() );
        return 1;
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
    init_pair( 12, COLOR_BLACK, COLOR_BLUE );
    init_pair( 13, COLOR_MAGENTA, COLOR_BLUE );
    init_pair( 14, COLOR_GREEN, COLOR_BLUE );
    init_pair( 15, COLOR_YELLOW, COLOR_BLUE );
    init_pair( 16, COLOR_WHITE, COLOR_RED );
    init_pair( 17, COLOR_RED, COLOR_CYAN );

    storage.WaitPreload();

    Browser browser( std::move( archive ), storage, galaxy.get(), lastOpen );
    browser.Entry();

    endwin();

    auto last = browser.GetArchiveFilename();
    storage.WriteArticleHistory( last );
    if( galaxy )
    {
        storage.WriteLastOpenGalaxyArchive( galaxy->GetActiveArchive() );
    }
    else
    {
        storage.WriteLastOpenArchive( last );
    }

    return 0;
}
