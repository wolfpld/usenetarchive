#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

#include "../contrib/martinus/robin_hood.h"
#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/MessageLogic.hpp"
#include "../common/MessageView.hpp"
#include "../common/RawImportMeta.hpp"
#include "../common/String.hpp"

static void Write( const MessageView& mview, uint32_t i, FILE* ddata, FILE* dmeta, uint64_t& offset )
{
    const auto raw = mview.Raw( i );
    fwrite( raw.ptr, 1, raw.compressedSize, ddata );

    RawImportMeta metaPacket = { offset, uint32_t( raw.size ), uint32_t( raw.compressedSize ) };
    fwrite( &metaPacket, 1, sizeof( RawImportMeta ), dmeta );
    offset += raw.compressedSize;
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
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    CreateDirStruct( argv[2] );

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const auto size = mview.Size();

    std::string dbase = argv[2];
    dbase.append( "/" );
    std::string dmetafn = dbase + "meta";
    std::string ddatafn = dbase + "data";

    FILE* dmeta = fopen( dmetafn.c_str(), "wb" );
    FILE* ddata = fopen( ddatafn.c_str(), "wb" );

    ExpandingBuffer eb;
    uint32_t cntd = 0;
    uint32_t cntu = 0;
    uint32_t cntb = 0;
    robin_hood::unordered_flat_set<std::string> unique;
    uint64_t offset = 0;
    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto buf = FindOptionalHeader( post, "message-id: ", 12 );
        if( *buf == '\n' )
        {
            buf = FindOptionalHeader( post, "message-id:\t", 12 );
        }
        if( *buf != '\n' )
        {
            buf += 12;
            auto end = buf;
            while( *buf != '<' && *buf != '\n' ) buf++;
            if( *buf == '\n' )
            {
                std::swap( end, buf );
            }
            else
            {
                buf++;
                end = buf;
                while( *end != '>' && *end != '\n' ) end++;
            }

            if( IsMsgId( buf, end ) )
            {
                std::string tmp( buf, end );
                if( unique.find( tmp ) == unique.end() )
                {
                    unique.emplace( std::move( tmp ) );
                    cntu++;
                    Write( mview, i, ddata, dmeta, offset );
                }
                else
                {
                    cntd++;
                }
            }
            else
            {
                Write( mview, i, ddata, dmeta, offset );
                cntb++;
            }
        }
        else
        {
            Write( mview, i, ddata, dmeta, offset );
            cntb++;
        }
    }

    fclose( dmeta );
    fclose( ddata );

    printf( "Processed %i MsgIDs. Unique: %i, dupes: %i, broken: %i\n", size, cntu, cntd, cntb );

    return 0;
}
