#include <algorithm>
#include <assert.h>
#include <unordered_set>
#include <random>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

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
    crm114_cb_setflags( crm_cb, CRM114_OSB_BAYES );
    crm114_cb_setclassdefaults( crm_cb );
    crm_cb->how_many_classes = 2;
    strcpy( crm_cb->cls[0].name, "valid" );
    strcpy( crm_cb->cls[1].name, "spam" );
    crm_cb->datablock_size = 1 << 25;
    crm114_cb_setblockdefaults( crm_cb );

    CRM114_DATABLOCK* crm_db = crm114_new_db( crm_cb );

    const std::string dbdir( argv[1] );
    if( Exists( dbdir + "/spamdb" ) )
    {
        FILE* f = fopen( ( dbdir + "/spamdb" ).c_str(), "rb" );
        fread( crm_db, 1, crm_cb->datablock_size, f );
        fclose( f );
    }

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

        std::string dbase = argv[3];
        dbase.append( "/" );
        std::string dmetafn = dbase + "meta";
        std::string ddatafn = dbase + "data";

        FILE* dmeta = fopen( dmetafn.c_str(), "wb" );
        FILE* ddata = fopen( ddatafn.c_str(), "wb" );

        printf( "Killed:\n" );

        uint64_t offset = 0;
        uint64_t savec = 0, saveu = 0;
        uint32_t cntbad = 0;
        for( uint32_t i=0; i<size; i++ )
        {
            const auto raw = mview.Raw( i );
            auto cdata = conn[i];
            auto parent = cdata[1];
            auto children = cdata[2];
            if( children == 0 && parent == -1 )
            {
                auto post = mview[i];
                CRM114_MATCHRESULT res;
                crm114_classify_text( crm_db, post, raw.size, &res );
                if( res.bestmatch_index != 0 )
                {
                    savec += raw.compressedSize;
                    saveu += raw.size;
                    cntbad++;
                    printf( "\033[33;1m%s\t\033[35;1m%s\033[0m\n", strings[i*3+1], strings[i*3] );
                    continue;
                }
            }

            fwrite( raw.ptr, 1, raw.compressedSize, ddata );

            RawImportMeta metaPacket = { offset, raw.size, raw.compressedSize };
            fwrite( &metaPacket, 1, sizeof( RawImportMeta ), dmeta );
            offset += raw.compressedSize;
        }

        fclose( dmeta );
        fclose( ddata );

        printf( "\nKilled %i messages.\nSaved %i KB (uncompressed), %i KB (compressed)\n", cntbad, saveu / 1024, savec / 1024 );
    }
    else
    {
        const MetaView<uint32_t, char> msgid( base + "midmeta", base + "middata" );
        std::unordered_set<std::string> visited;

        if( !Exists( dbdir ) )
        {
            CreateDirStruct( dbdir );
        }
        else if( Exists( dbdir + "/visited" ) )
        {
            ExpandingBuffer buf;
            FILE* f = fopen( ( dbdir + "/visited" ).c_str(), "rb" );
            uint32_t num;
            fread( &num, 1, sizeof( uint32_t ), f );
            while( num-- )
            {
                uint32_t size;
                fread( &size, 1, sizeof( uint32_t ), f );
                auto tmp = buf.Request( size );
                fread( tmp, 1, size, f );
                visited.emplace( tmp, tmp+size );
            }
            fclose( f );
        }

        printf( "Spam training mode.\n" );

        std::vector<uint32_t> indices;
        for( uint32_t i=0; i<topsize; i++ )
        {
            auto idx = toplevel[i];
            auto children = conn[idx];
            children += 2;
            if( *children != 0 ) continue;
            std::string id( msgid[idx] );
            if( visited.find( id ) != visited.end() ) continue;
            indices.push_back( idx );
        }

        std::random_device rd;
        std::mt19937 rng( rd() );
        std::shuffle( indices.begin(), indices.end(), rng );

        for( auto& idx : indices )
        {
            visited.emplace( msgid[idx] );

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

            for( int i=0; i<20; i++ )
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
            if( *end != '\0' )
            {
                printf( "...\n" );
            }

            CRM114_MATCHRESULT res;
            crm114_classify_text( crm_db, post, raw.size, &res );

            if( res.bestmatch_index == 0 )
            {
                printf( "\033[32;1mClassification: valid" );
            }
            else
            {
                printf( "\033[31;1mClassification: spam" );
            }
            printf( "   success probability: %.3f.\n", res.tsprob );
            printf( "Press [s] for spam or [v] for valid, [i] to ignore. Press [W] to write database or [Q] to quit.\033[0m\n" );
            fflush( stdout );

            char c;
            do
            {
                c = getchar();
            }
            while( c != 's' && c != 'v' && c != 'Q' && c != 'W' && c != 'i' );

            if( c == 'Q' )
            {
                return 0;
            }
            else if( c == 'W' )
            {
                break;
            }
            else if( c != 'i' )
            {
                crm114_learn_text( &crm_db, (c == 's') ? 1 : 0, post, raw.size );
            }
        }

        FILE* fdb = fopen( ( dbdir + "/spamdb" ).c_str(), "wb" );
        fwrite( crm_db, 1, crm_cb->datablock_size, fdb );
        fclose( fdb );

        FILE* fvis = fopen( ( dbdir + "/visited" ).c_str(), "wb" );
        uint32_t n = visited.size();
        fwrite( &n, 1, sizeof( uint32_t ), fvis );
        for( auto& v : visited )
        {
            n = v.size();
            fwrite( &n, 1, sizeof( uint32_t ), fvis );
            fwrite( v.data(), 1, v.size(), fvis );
        }
        fclose( fvis );
    }

    return 0;
}
