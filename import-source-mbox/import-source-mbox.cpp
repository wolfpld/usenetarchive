#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>

#include "../contrib/lz4/lz4.h"
#include "../contrib/lz4/lz4hc.h"
#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/RawImportMeta.hpp"

static bool ReadLine( FILE* f, std::string& out )
{
    out.clear();

    for(;;)
    {
        char c;
        const size_t read = fread( &c, 1, 1, f );
        if( read == 0 || c == '\n' )
        {
            return read > 0 || !out.empty();
        }
        if( c != '\r' )
        {
            out += c;
        }
    }
}

int main( int argc, char** argv )
{
    if( argc != 3 )
    {
        fprintf( stderr, "USAGE: %s input output\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source file doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[2] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    CreateDirStruct( argv[2] );
    FILE* in = fopen( argv[1], "rb" );

    std::string metafn = argv[2];
    metafn.append( "/" );
    std::string datafn = metafn;
    metafn.append( "meta" );
    datafn.append( "data" );
    FILE* meta = fopen( metafn.c_str(), "wb" );
    FILE* data = fopen( datafn.c_str(), "wb" );

    uint64_t offset = 0;

    ExpandingBuffer eb1, eb2;
    std::string line;
    std::string post;
    int idx = 0;
    while( ReadLine( in, line ) )
    {
        if( line.size() > 5 && strncmp( line.data(), "From ", 5 ) == 0 )
        {
            post.clear();
            int lines = -1;
            while( ReadLine( in, line ) )
            {
                if( strncmp( line.data(), "Lines: ", 7 ) == 0 )
                {
                    lines = atoi( line.c_str() + 7 );
                }
                post.append( line );
                post.append( "\n" );
                if( line.empty() ) break;
            }
            if( lines != -1 )
            {
                while( lines-- )
                {
                    ReadLine( in, line );
                    post.append( line );
                    post.append( "\n" );
                }

                int maxSize = LZ4_compressBound( post.size() );
                char* compressed = eb2.Request( maxSize );
                int csize = LZ4_compress_HC( post.data(), compressed, post.size(), maxSize, 16 );

                fwrite( compressed, 1, csize, data );

                RawImportMeta metaPacket = { offset, post.size(), csize };
                fwrite( &metaPacket, 1, sizeof( RawImportMeta ), meta );
                offset += csize;

                if( ( idx & 0x3FF ) == 0 )
                {
                    printf( "%i\r", idx );
                    fflush( stdout );
                }
                idx++;
            }
        }
    }
    printf( "%i files processed.\n", idx );

    fclose( meta );
    fclose( data );

    return 0;
}
