#ifdef _WIN32
#  include <windows.h>
#endif

#include <algorithm>
#include <assert.h>
#include <random>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

#include "../contrib/terminator/terminator.h"
#include "../contrib/martinus/robin_hood.h"

#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/HashSearch.hpp"
#include "../common/MessageView.hpp"
#include "../common/MetaView.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"
#include "../common/StringCompress.hpp"
#include "../common/ZMessageView.hpp"

int main( int argc, char** argv )
{
#ifdef _WIN32
    SetConsoleMode( GetStdHandle( STD_OUTPUT_HANDLE ), 0x07 );
#endif

    if( argc < 3 )
    {
        fprintf( stderr, "USAGE: %s database source [destination ( [--ask] | [--quiet] | [--size max] | [--kill msgid...] | [--thread threshold] )] | [-m msgid]\n", argv[0] );
        fprintf( stderr, "Omitting destination will start training mode.\n" );
        exit( 1 );
    }

    bool training = false;
    bool quiet = false;
    bool ask = false;
    bool kill = false;
    int maxsize = -1;
    float threadThreshold = -1;
    bool thread = false;

    std::vector<const char*> toKill;

    if( argc > 4 )
    {
        if( strcmp( argv[3], "-m" ) == 0 )
        {
            training = true;
        }
        else if( strcmp( argv[4], "--quiet" ) == 0 )
        {
            quiet = true;
        }
        else if( strcmp( argv[4], "--ask" ) == 0 )
        {
            ask = true;
        }
        else if( strcmp( argv[4], "--size" ) == 0 )
        {
            if( argc != 6 )
            {
                fprintf( stderr, "No max size provided.\n" );
                exit( 1 );
            }
            maxsize = atoi( argv[5] );
        }
        else if( strcmp( argv[4], "--kill" ) == 0 )
        {
            kill = true;
            for( int i=5; i<argc; i++ )
            {
                toKill.emplace_back( argv[i] );
            }
        }
        else if( strcmp( argv[4], "--thread" ) == 0 )
        {
            thread = true;
            threadThreshold = atof( argv[5] );
        }
        else
        {
            fprintf( stderr, "Bad params!\n" );
            exit( 1 );
        }
    }
    else if( argc == 3 )
    {
        training = true;
    }

    if( !Exists( argv[2] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }

    std::string base = argv[2];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const auto size = mview.Size();
    const FileMap<uint32_t> toplevel( base + "toplevel" );
    auto topsize = toplevel.DataSize();
    const MetaView<uint32_t, char> strings( base + "strmeta", base + "strings" );
    const MetaView<uint32_t, uint32_t> conn( base + "connmeta", base + "conndata" );
    const MetaView<uint32_t, uint8_t> msgid( base + "midmeta", base + "middata" );
    const HashSearch<uint8_t> midhash( base + "middata", base + "midhash", base + "midhashdata" );
    const StringCompress compress( base + "msgid.codebook" );

    const std::string dbdir( argv[1] );
    if( !Exists( dbdir ) )
    {
        CreateDirStruct( dbdir );
    }

    auto classifier = std::make_unique<Terminator>( dbdir + "/spamdb", 16*1024*1024 );

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

        CopyCommonFiles( base, dbase );

        FILE *dzmeta = nullptr;
        FILE* dzdata = nullptr;
        ZMessageView* zview = nullptr;
        if( Exists( base + "zdict" ) )
        {
            zview = new ZMessageView( base + "zmeta", base + "zdata", base + "zdict" );
            dzmeta = fopen( ( dbase + "zmeta" ).c_str(), "wb" );
            dzdata = fopen( ( dbase + "zdata" ).c_str(), "wb" );

            if( !dzmeta || !dzdata )
            {
                fprintf( stderr, "Incomplete zstd data.\n" );
                exit( 1 );
            }
            if( zview->Size() != size )
            {
                fprintf( stderr, "LZ4 and zstd message number mismatch.\n" );
                exit( 1 );
            }

            CopyFile( base + "zdict", dbase + "zdict" );
        }

        FILE* dmeta = fopen( dmetafn.c_str(), "wb" );
        FILE* ddata = fopen( ddatafn.c_str(), "wb" );

        struct Data
        {
            uint32_t id;
            float prob;
        };

        std::vector<Data> data;

        for( uint32_t i=0; i<size; i++ )
        {
            if( ( i & 0xFF ) == 0 )
            {
                printf( "%i/%i\r", i, size );
                fflush( stdout );
            }

            const auto raw = mview.Raw( i );
            if( maxsize != -1 )
            {
                if( raw.size > maxsize )
                {
                    data.emplace_back( Data { i, float( raw.size ) } );
                }
            }
            else if( kill )
            {
                char id[2048];
                compress.Unpack( msgid[i], id );
                for( auto& v : toKill )
                {
                    if( strcmp( id, v ) == 0 )
                    {
                        data.emplace_back( Data { i, float( raw.size ) } );
                        break;
                    }
                }
            }
            else
            {
                auto cdata = conn[i];
                auto parent = cdata[1];
                if( parent == -1 )
                {
                    auto children = cdata[3];
                    if( children == 0 )
                    {
                        auto post = mview[i];
                        auto score = classifier->Predict( post );
                        if( score > 0.5 )
                        {
                            data.emplace_back( Data { i, float( score ) } );
                        }
                    }
                    else if( thread )
                    {
                        bool allBad = true;
                        auto toCheck = cdata[2];
                        std::vector<float> scores;
                        scores.reserve( toCheck );
                        for( int j=0; j<toCheck; j++ )
                        {
                            const auto craw = mview.Raw( i+j );
                            auto post = mview[i+j];
                            auto score = classifier->Predict( post );
                            if( score < threadThreshold )
                            {
                                allBad = false;
                                break;
                            }
                            else
                            {
                                scores.emplace_back( float( score ) );
                            }
                        }
                        if( allBad )
                        {
                            for( int j=0; j<toCheck; j++ )
                            {
                                data.emplace_back( Data { i+j, scores[j] } );
                            }
                        }
                    }
                }
            }
        }

        if( !quiet )
        {
            printf( "\n%i messages marked as spam. Killed:\n", data.size() );
        }

        uint64_t offset = 0;
        uint64_t zoffset = 0;
        uint64_t savec = 0, saveu = 0;
        uint32_t cntbad = 0;
        auto it = data.begin();
        for( uint32_t i=0; i<size; i++ )
        {
            const auto raw = mview.Raw( i );

            if( it != data.end() && it->id == i )
            {
                if( !quiet )
                {
                    char unpack[2048];
                    compress.Unpack( msgid[i], unpack );
                    if( maxsize == -1 )
                    {
                        int color;
                        if( it->prob < 0.35f )
                        {
                            color = 32;
                        }
                        else if( it->prob <= 0.65f )
                        {
                            color = 36;
                        }
                        else
                        {
                            color = 31;
                        }
                        printf( "\033[%i;1m%.3f \033[33;1m%s\t\033[35;1m%s\t\033[0m%s\n", color, it->prob, strings[i*3+1], strings[i*3], unpack );
                    }
                    else
                    {
                        printf( "\033[33;1m%s\t\033[35;1m%s\t\033[36;1m%i\033[0m\t%s\n", strings[i*3+1], strings[i*3], (int)it->prob, unpack );
                    }
                    fflush( stdout );
                }
                ++it;
                bool spam = true;
                if( ask )
                {
                    printf( "\033[31;1mSelect [s]pam or [v]alid.\033[0m\n" );
                    fflush( stdout );
                    char c;
                    do
                    {
                        c = getchar();
                    }
                    while( c != 's' && c != 'v' );
                    if( c == 'v' ) spam = false;
                }
                if( spam )
                {
                    savec += raw.compressedSize;
                    saveu += raw.size;
                    cntbad++;
                    continue;
                }
            }

            fwrite( raw.ptr, 1, raw.compressedSize, ddata );

            RawImportMeta metaPacket = { offset, uint32_t( raw.size ), uint32_t( raw.compressedSize ) };
            fwrite( &metaPacket, 1, sizeof( RawImportMeta ), dmeta );
            offset += raw.compressedSize;

            if( zview )
            {
                const auto zraw = zview->Raw( i );
                if( zraw.size != raw.size )
                {
                    fprintf( stderr, "LZ4 and zstd message %i size mismatch\n", i );
                    exit( 1 );
                }

                RawImportMeta zpacket = { zoffset, uint32_t( zraw.size ), uint32_t( zraw.compressedSize ) };
                fwrite( &zpacket, 1, sizeof( RawImportMeta ), dzmeta );

                fwrite( zraw.ptr, 1, zraw.compressedSize, dzdata );
                zoffset += zraw.compressedSize;
            }
        }

        fclose( dmeta );
        fclose( ddata );

        if( dzmeta ) fclose( dzmeta );
        if( dzdata ) fclose( dzdata );

        delete zview;

        printf( "\nKilled %i messages.\nSaved %i KB (uncompressed), %i KB (compressed)\n", cntbad, saveu / 1024, savec / 1024 );
    }
    else
    {
        robin_hood::unordered_flat_set<std::string> visited;

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
        if( argc == 5 )
        {
            uint8_t pack[2048];
            compress.Pack( argv[4], pack );
            auto idx = midhash.Search( pack );
            if( idx == -1 )
            {
                fprintf( stderr, "Invalid MsgID!\n" );
                exit( 1 );
            }
            indices.push_back( idx );
        }
        else
        {
            for( uint32_t i=0; i<topsize; i++ )
            {
                auto idx = toplevel[i];
                auto children = conn[idx];
                children += 3;
                if( *children != 0 ) continue;
                char unpack[2048];
                compress.Unpack( msgid[idx], unpack );
                std::string id( unpack );
                if( visited.find( id ) != visited.end() ) continue;
                indices.push_back( idx );
            }

            std::random_device rd;
            std::mt19937 rng( rd() );
            std::shuffle( indices.begin(), indices.end(), rng );
        }

        for( auto& idx : indices )
        {
            char unpack[2048];
            compress.Unpack( msgid[idx], unpack );
            visited.emplace( unpack );

            auto post = mview[idx];
            auto raw = mview.Raw( idx );

            time_t date = *conn[idx];
            auto lt = localtime( &date );
            char buf[64];
            auto dlen = strftime( buf, 64, "%F %R", lt );

            printf("\033[2J\033[0;0H");

            printf( "\n\t\033[36;1m-= Message %i =-\033[0m\n\n", idx );
            printf( "\033[35;1mFrom: %s\n\033[33;1mSubject: %s\n\033[34mDate: %s\n\033[36mMessage-Id: %s\033[0m\n\n", strings[idx*3], strings[idx*3+1], buf, unpack );

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

            for( int i=0; i<30; i++ )
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

            std::string str( post );
            auto score = classifier->Predict( str );

            if( score < 0.35 )
            {
                printf( "\033[32;1mClassification: valid" );
            }
            else if( score <= 0.5 )
            {
                printf( "\033[36;1mClassification: unsure/valid" );
            }
            else if( score <= 0.65 )
            {
                printf( "\033[36;1mClassification: unsure/spam" );
            }
            else
            {
                printf( "\033[31;1mClassification: spam" );
            }
            printf( "   message score: %.3f.\n", score );
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
                classifier->Train( str, c == 's' );
            }
        }

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
