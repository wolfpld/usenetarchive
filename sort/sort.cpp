#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/LexiconTypes.hpp"
#include "../common/MessageView.hpp"
#include "../common/MetaView.hpp"
#include "../common/RawImportMeta.hpp"

int Expand( int idx, std::vector<uint32_t>& order, const uint32_t* data, const MetaView<uint32_t, uint32_t>& conn )
{
    int cskip = 1;
    data += 3;
    auto num = *data++;
    idx++;
    for( int i=0; i<num; i++ )
    {
        auto child = data[i];
        auto cdata = conn[child];
        auto skip = cdata[2];
        order[idx] = child;
        auto ret = Expand( idx, order, cdata, conn );
        assert( ret == skip );
        idx += skip;
        cskip += skip;
    }
    return cskip;
}

int main( int argc, char** argv )
{
    if( argc != 3 )
    {
        fprintf( stderr, "USAGE: %s source destination\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[2] ) )
    {
        fprintf( stderr, "Destination directory exists.\n" );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );

    MetaView<uint32_t, uint32_t> conn( base + "connmeta", base + "conndata" );
    FileMap<uint32_t> toplevel( base + "toplevel" );

    printf( "Sorting..." );
    fflush( stdout );
    const auto size = conn.Size();
    std::vector<uint32_t> order( size );
    std::vector<uint32_t> revtop( toplevel.DataSize() );
    unsigned int idx = 0;
    for( int i=0; i<toplevel.DataSize(); i++ )
    {
        order[idx] = toplevel[i];
        revtop[i] = idx;
        auto data = conn[toplevel[i]];
        auto ret = Expand( idx, order, data, conn );
        assert( ret == data[2] );
        idx += data[2];
    }
    assert( idx == size );

    std::vector<uint32_t> rev( size );
    for( int i=0; i<size; i++ )
    {
        rev[order[i]] = i;
    }

    std::string dbase = argv[2];
    CreateDirStruct( dbase );
    dbase.append( "/" );

    printf( " done\nCopy common files..." );
    fflush( stdout );

    CopyCommonFiles( base, dbase );

    CopyFile( base + "strings", dbase + "strings" );

    CopyFile( base + "middata", dbase + "middata" );
    CopyFile( base + "midhashdata", dbase + "midhashdata" );
    CopyFile( base + "msgid.codebook", dbase + "msgid.codebook" );

    CopyFile( base + "lexhash", dbase + "lexhash" );
    CopyFile( base + "lexhashdata", dbase + "lexhashdata" );
    CopyFile( base + "lexhit", dbase + "lexhit" );
    CopyFile( base + "lexmeta", dbase + "lexmeta" );
    CopyFile( base + "lexstr", dbase + "lexstr" );

    printf( " done\n" );

    if( Exists( base + "meta" ) && Exists( base + "data" ) )
    {
        MessageView mview( base + "meta", base + "data" );

        std::string dmetafn = dbase + "meta";
        std::string ddatafn = dbase + "data";

        FILE* dmeta = fopen( dmetafn.c_str(), "wb" );
        FILE* ddata = fopen( ddatafn.c_str(), "wb" );

        uint64_t offset = 0;
        for( int i=0; i<size; i++ )
        {
            if( ( i & 0x3FF ) == 0 )
            {
                printf( "LZ4 %i/%i\r", i, size );
                fflush( stdout );
            }

            auto raw = mview.Raw( order[i] );
            fwrite( raw.ptr, 1, raw.compressedSize, ddata );

            RawImportMeta metaPacket = { offset, raw.size, raw.compressedSize };
            fwrite( &metaPacket, 1, sizeof( RawImportMeta ), dmeta );
            offset += raw.compressedSize;
        }

        printf( "\n" );
    }

    if( Exists( base + "zmeta" ) && Exists( base + "zdata" ) && Exists( base + "zdict" ) )
    {
        CopyFile( base + "zdict", dbase + "zdict" );

        // Hack! This should be ZMessageView, but we only use common data addressing,
        // so MessageView works here.
        MessageView mview( base + "zmeta", base + "zdata" );

        std::string dmetafn = dbase + "zmeta";
        std::string ddatafn = dbase + "zdata";

        FILE* dmeta = fopen( dmetafn.c_str(), "wb" );
        FILE* ddata = fopen( ddatafn.c_str(), "wb" );

        uint64_t offset = 0;
        for( int i=0; i<size; i++ )
        {
            if( ( i & 0x3FF ) == 0 )
            {
                printf( "zstd %i/%i\r", i, size );
                fflush( stdout );
            }

            auto raw = mview.Raw( order[i] );
            fwrite( raw.ptr, 1, raw.compressedSize, ddata );

            RawImportMeta metaPacket = { offset, raw.size, raw.compressedSize };
            fwrite( &metaPacket, 1, sizeof( RawImportMeta ), dmeta );
            offset += raw.compressedSize;
        }

        printf( "\n" );
    }

    {
        FILE* dst = fopen( ( dbase + "toplevel" ).c_str(), "wb" );
        for( int i=0; i<toplevel.DataSize(); i++ )
        {
            if( ( i & 0xFFF ) == 0 )
            {
                printf( "toplevel %i/%i\r", i, toplevel.DataSize() );
                fflush( stdout );
            }
            fwrite( &revtop[i], 1, sizeof( uint32_t ), dst );
        }
        fclose( dst );
        printf( "\n" );
    }

    {
        FILE* data = fopen( ( dbase + "conndata" ).c_str(), "wb" );
        FILE* meta = fopen( ( dbase + "connmeta" ).c_str(), "wb" );
        uint32_t offset = 0;
        for( int i=0; i<conn.Size(); i++ )
        {
            if( ( i & 0x3FF ) == 0 )
            {
                printf( "conn %i/%i\r", i, size );
                fflush( stdout );
            }
            fwrite( &offset, 1, sizeof( offset ), meta );

            auto src = conn[order[i]];
            offset += fwrite( src++, 1, sizeof( uint32_t ), data );     // epoch
            int32_t parent = *src++;
            if( parent != -1 ) parent = rev[parent];
            offset += fwrite( &parent, 1, sizeof( int32_t ), data );
            offset += fwrite( src++, 1, sizeof( uint32_t ), data );     // child total
            auto num = *src;
            offset += fwrite( src++, 1, sizeof( uint32_t ), data );     // child num
            for( int j=0; j<num; j++ )
            {
                auto child = rev[*src++];
                offset += fwrite( &child, 1, sizeof( uint32_t ), data );
            }
        }
        fclose( data );
        fclose( meta );
        printf( "\n" );
    }

    {
        FileMap<uint32_t> midmeta( base + "midmeta" );
        FILE* dst = fopen( ( dbase + "midmeta" ).c_str(), "wb" );
        for( int i=0; i<size; i++ )
        {
            if( ( i & 0xFFF ) == 0 )
            {
                printf( "midmeta %i/%i\r", i, size );
                fflush( stdout );
            }
            fwrite( midmeta + order[i], 1, sizeof( uint32_t ), dst );
        }
        fclose( dst );
        printf( "\n" );
    }

    {
        FileMap<uint32_t> midhash( base + "midhash" );
        const auto hsize = midhash.DataSize() / 2;
        FILE* dst = fopen( ( dbase + "midhash" ).c_str(), "wb" );
        for( int i=0; i<hsize; i++ )
        {
            if( ( i & 0xFFF ) == 0 )
            {
                printf( "midhash %i/%i\r", i, hsize );
                fflush( stdout );
            }
            if( midhash[i*2] > 0 )
            {
                uint32_t idx = rev[midhash[i*2+1]];
                fwrite( midhash+i*2, 1, sizeof( uint32_t ), dst );
                fwrite( &idx, 1, sizeof( uint32_t ), dst );
            }
            else
            {
                uint64_t zero = 0;
                fwrite( &zero, 1, sizeof( uint64_t ), dst );
            }
        }
        fclose( dst );
        printf( "\n" );
    }

    {
        FileMap<LexiconMetaPacket> lexmeta( base + "lexmeta" );
        FileMap<LexiconDataPacket> lexdata( base + "lexdata" );

        FILE* dst = fopen( ( dbase + "lexdata" ).c_str(), "wb" );
        for( int i=0; i<lexmeta.DataSize(); i++ )
        {
            if( ( i & 0x3FF ) == 0 )
            {
                printf( "lexdata %i/%i\r", i, lexmeta.DataSize() );
                fflush( stdout );
            }
            auto meta = lexmeta[i];
            auto data = lexdata + ( meta.data / sizeof( LexiconDataPacket ) );
            for( int j=0; j<meta.dataSize; j++ )
            {
                auto idx = rev[data->postid & LexiconPostMask] | ( data->postid & LexiconChildMask );
                fwrite( &idx, 1, sizeof( idx ), dst );
                fwrite( &data->hitoffset, 1, sizeof( data->hitoffset ), dst );
                data++;
            }
        }
        fclose( dst );
        printf( "\n" );
    }

    {
        FileMap<uint32_t> strmeta( base + "strmeta" );

        FILE* dst = fopen( ( dbase + "strmeta" ).c_str(), "wb" );
        for( int i=0; i<size; i++ )
        {
            if( ( i & 0x3FF ) == 0 )
            {
                printf( "strmeta %i/%i\r", i, size );
                fflush( stdout );
            }
            fwrite( strmeta + order[i] * 3, 1, sizeof( uint32_t ) * 3, dst );
        }
        fclose( dst );
        printf( "\n" );
    }

    printf( "Note: Remember to run uat-lexsort.\n" );

    return 0;
}
