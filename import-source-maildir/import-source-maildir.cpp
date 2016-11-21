#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#ifdef _WIN32
#  include <direct.h>
#  include <windows.h>
#else
#  include <dirent.h>
#endif

#include "../contrib/lz4/lz4.h"
#include "../contrib/lz4/lz4hc.h"
#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/RawImportMeta.hpp"

static std::vector<std::string> ListDirectory( const std::string& path )
{
    std::vector<std::string> ret;

#ifdef _WIN32
    WIN32_FIND_DATA ffd;
    HANDLE h;

    std::string p = path + "/*";
    for( unsigned int i=0; i<p.size(); i++ )
    {
        if( p[i] == '/' )
        {
            p[i] = '\\';
        }
    }

    h = FindFirstFile( ( p ).c_str(), &ffd );
    if( h == INVALID_HANDLE_VALUE )
    {
        return ret;
    }

    do
    {
        std::string s = ffd.cFileName;
        if( s != "." && s != ".." )
        {
            if( ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                s += "/";
            }
            ret.emplace_back( std::move( s ) );
        }
    }
    while( FindNextFile( h, &ffd ) );

    FindClose( h );
#else
    DIR* dir = opendir( path.c_str() );
    if( dir == nullptr )
    {
        return ret;
    }

    struct dirent* ent;
    while( ( ent = readdir( dir ) ) != nullptr )
    {
        std::string s = ent->d_name;
        if( s != "." && s != ".." )
        {
            if( ent->d_type == DT_DIR )
            {
                s += "/";
            }
            ret.emplace_back( std::move( s ) );
        }
    }
    closedir( dir );
#endif

    return ret;
}

static int idx = 0;
static int dirs = 0;
static FILE* meta;
static FILE* data;
static uint64_t offset = 0;
static ExpandingBuffer eb1, eb2;

void RecursivePack( const char* dir )
{
    dirs++;
    const auto list = ListDirectory( dir );
    char in[1024];
    int fpos = strlen( dir );
    memcpy( in, dir, fpos );
    in[fpos++] = '/';

    for( const auto& f : list )
    {
        if( ( idx & 0x3FF ) == 0 )
        {
            printf( "[%i] %i\r", dirs, idx );
            fflush( stdout );
        }

        if( f[0] == '.' ) continue;
        strcpy( in+fpos, f.c_str() );
        if( f.back() == '/' )
        {
            RecursivePack( in );
        }
        else
        {
            idx++;
            uint64_t size = GetFileSize( in );
            char* buf = eb1.Request( size );
            FILE* src = fopen( in, "rb" );
            fread( buf, 1, size, src );
            fclose( src );

            int maxSize = LZ4_compressBound( size );
            char* compressed = eb2.Request( maxSize );
            int csize = LZ4_compress_HC( buf, compressed, size, maxSize, 16 );

            fwrite( compressed, 1, csize, data );

            RawImportMeta metaPacket = { offset, size, csize };
            fwrite( &metaPacket, 1, sizeof( RawImportMeta ), meta );
            offset += csize;
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
        fprintf( stderr, "Source directory doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[2] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

    CreateDirStruct( argv[2] );

    std::string metafn = argv[2];
    metafn.append( "/" );
    std::string datafn = metafn;
    metafn.append( "meta" );
    datafn.append( "data" );
    meta = fopen( metafn.c_str(), "wb" );
    data = fopen( datafn.c_str(), "wb" );

    RecursivePack( argv[1] );

    printf( "%i files processed in %i directories.\n", idx, dirs );

    fclose( meta );
    fclose( data );

    return 0;
}
