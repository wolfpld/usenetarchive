#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "../common/Filesystem.hpp"
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

void CopyFile( const std::string& from, const std::string& to )
{
    assert( Exists( from ) );
    FILE* src = fopen( from.c_str(), "rb" );
    FILE* dst = fopen( to.c_str(), "wb" );
    assert( src && dst );
    enum { BlockSize = 16 * 1024 };
    char buf[BlockSize];
    for(;;)
    {
        auto read = fread( buf, 1, BlockSize, src );
        fwrite( buf, 1, read, dst );
        if( read != BlockSize ) break;
    }
    fclose( src );
    fclose( dst );
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

    const auto size = conn.Size();
    std::vector<uint32_t> order( size );
    unsigned int idx = 0;
    for( int i=0; i<toplevel.DataSize(); i++ )
    {
        order[idx] = toplevel[i];
        auto data = conn[toplevel[i]];
        auto ret = Expand( idx, order, data, conn );
        assert( ret == data[2] );
        idx += data[2];
    }
    assert( idx == size );

    std::string dbase = argv[2];
    CreateDirStruct( dbase );
    dbase.append( "/" );

    printf( "Copy common files..." );
    fflush( stdout );

    if( Exists( base + "name" ) ) CopyFile( base + "name", dbase + "name" );
    if( Exists( base + "desc_long" ) ) CopyFile( base + "desc_long", dbase + "desc_long" );
    if( Exists( base + "desc_short" ) ) CopyFile( base + "desc_short", dbase + "desc_short" );
    if( Exists( base + "strings" ) ) CopyFile( base + "strings", dbase + "strings" );
    if( Exists( base + "strmeta" ) ) CopyFile( base + "strmeta", dbase + "strmeta" );
    CopyFile( base + "conndata", dbase + "conndata" );

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
        MessageView mview( base + "meta", base + "data" );

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
            fwrite( &order[toplevel[i]], 1, sizeof( uint32_t ), dst );
        }
        fclose( dst );
        printf( "\n" );
    }

    {
        FileMap<uint32_t> connmeta( base + "connmeta" );
        std::vector<uint32_t> v( size );
        for( int i=0; i<size; i++ )
        {
            if( ( i & 0xFFF ) == 0 )
            {
                printf( "connmeta %i/%i\r", i, size );
                fflush( stdout );
            }
            v[order[i]] = connmeta[i];
        }
        FILE* dst = fopen( ( dbase + "connmeta" ).c_str(), "wb" );
        fwrite( v.data(), 1, size * sizeof( uint32_t ), dst );
        fclose( dst );
        printf( "\n" );
    }

    return 0;
}
