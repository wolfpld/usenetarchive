#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/MessageView.hpp"
#include "../common/MetaView.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"

extern "C" {
#include "../contrib/libcrm114/crm114_sysincludes.h"
#include "../contrib/libcrm114/crm114_config.h"
#include "../contrib/libcrm114/crm114_structs.h"
#include "../contrib/libcrm114/crm114_lib.h"
#include "../contrib/libcrm114/crm114_internal.h"
}

#ifdef _WIN32
#  include <windows.h>
#  include <conio.h>
#endif

int main( int argc, char** argv )
{
#ifdef _WIN32
    SetConsoleMode( GetStdHandle( STD_OUTPUT_HANDLE ), 0x07 );
#endif

    if( argc != 3 && argc != 4 )
    {
        fprintf( stderr, "USAGE: %s database source [destination]\n", argv[0] );
        fprintf( stderr, "Omitting destination will start training mode.\n" );
        exit( 1 );
    }

    bool training = argc == 3;

    if( !Exists( argv[2] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }

    std::string base = argv[2];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const auto size = mview.Size();
    FileMap<uint32_t> toplevel( base + "toplevel" );
    auto topsize = toplevel.DataSize();
    MetaView<uint32_t, char> strings( base + "strmeta", base + "strings" );
    MetaView<uint32_t, uint32_t> conn( base + "connmeta", base + "conndata" );

    CRM114_CONTROLBLOCK* crm_cb = crm114_new_cb();
    crm114_cb_setflags( crm_cb, CRM114_FSCM );
    crm114_cb_setclassdefaults( crm_cb );
    crm_cb->how_many_classes = 2;
    strcpy( crm_cb->cls[0].name, "valid" );
    strcpy( crm_cb->cls[1].name, "spam" );
    crm_cb->datablock_size = 1 << 25;
    crm114_cb_setblockdefaults( crm_cb );

    CRM114_DATABLOCK* crm_db = crm114_new_db( crm_cb );

    if( !training )
    {
        if( !Exists( argv[1] ) )
        {
            fprintf( stderr, "Spam database not present.\n" );
            exit( 1 );
        }
        if( Exists( argv[3] ) )
        {
            fprintf( stderr, "Destination directory already exists.\n" );
            exit( 1 );
        }

        CreateDirStruct( argv[3] );
    }
    else
    {
        if( !Exists( argv[1] ) )
        {
            CreateDirStruct( argv[1] );
        }

        printf( "Spam training mode.\n" );

        for( uint32_t i=0; i<topsize; i++ )
        {
            auto idx = toplevel[i];
            auto children = conn[idx];
            children += 2;
            if( *children != 0 ) continue;

            auto post = mview[idx];
            auto raw = mview.Raw( idx );

            printf("\033[2J\033[0;0H");

            printf( "\n\t\033[36;1m-= Message %i =-\033[0m\n\n", idx );
            printf( "\033[35;1mFrom: %s\n\033[33;1mSubject: %s\033[0m\n\n", strings[idx*3], strings[idx*3+1] );

            auto ptr = post;
            auto end = ptr;
            bool done = false;
            do
            {
                end = ptr;
                while( *end != '\n' ) end++;
                if( end - ptr == 0 )
                {
                    while( *end == '\n' ) end++;
                    end--;
                    done = true;
                }
                ptr = end + 1;
            }
            while( !done );

            for( int i=0; i<8; i++ )
            {
                end = ptr;
                while( *end != '\n' && *end != '\0' ) end++;
                while( ptr != end )
                {
                    fputc( *ptr++, stdout );
                }
                fputc( '\n', stdout );
                if( *end == '\0' ) break;
                ptr = end + 1;
            }

            CRM114_MATCHRESULT res;
            crm114_classify_text( crm_db, post, raw.size, &res );

            if( res.bestmatch_index == 0 )
            {
                printf( "\033[32mClassification: valid" );
            }
            else
            {
                printf( "\033[31mClassification: spam" );
            }
            printf( "   success probability: %.3f.\033[0m\n", res.tsprob );
            printf( "Press [s] for spam or [v] for valid.\n" );

            char c;
            do
            {
#ifdef _WIN32
                c = getch();
#else
                c = getchar();
#endif
            }
            while( c != 's' && c != 'v' );

            crm114_learn_text( &crm_db, (c == 's') ? 1 : 0, post, raw.size );
        }
    }

    return 0;
}
