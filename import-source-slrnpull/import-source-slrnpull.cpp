#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <direct.h>
#  include <windows.h>
#else
#  include <dirent.h>
#endif

#include "../common/RawImportMeta.hpp"

static bool Exists( const std::string& path )
{
    struct stat buf;
    return stat( path.c_str(), &buf ) == 0;
}

static uint64_t GetFileSize( const char* path )
{
    struct stat buf;
    stat( path, &buf );
    return buf.st_size;
}

static bool CreateDirStruct( const std::string& path )
{
    if( Exists( path ) ) return true;

    if( errno != ENOENT )
    {
        fprintf( stderr, "%s\n", strerror( errno ) );
        return false;
    }

    size_t pos = 0;
    do
    {
        pos = path.find( '/', pos+1 );
#ifdef _WIN32
        if( pos == 2 ) continue;    // Don't create drive name.
        if( _mkdir( path.substr( 0, pos ).c_str() ) != 0 )
#else
        if( mkdir( path.substr( 0, pos ).c_str(), S_IRWXU ) != 0 )
#endif
        {
            if( errno != EEXIST )
            {
                fprintf( stderr, "Creating failed on %s (%s)\n", path.substr( 0, pos ).c_str(), strerror( errno ) );
                return false;
            }
        }
    }
    while( pos != std::string::npos );

    return true;
}

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
    const auto list = ListDirectory( argv[1] );

    std::string metafn = argv[2];
    metafn.append( "/" );
    std::string datafn = metafn;
    metafn.append( "meta" );
    datafn.append( "data" );
    FILE* meta = fopen( metafn.c_str(), "wb" );
    FILE* data = fopen( datafn.c_str(), "wb" );

    uint64_t offset = 0;

    char in[1024];
    int idx = 0;
    for( const auto& f : list )
    {
        if( ( idx & 0x3FF ) == 0 )
        {
            printf( "%i/%i\r", idx, list.size() );
            fflush( stdout );
        }
        idx++;

        if( f[0] == '.' ) continue;

        sprintf( in, "%s/%s", argv[1], f.c_str() );
        uint64_t size = GetFileSize( in );

        RawImportMeta metaPacket = { offset, size };
        fwrite( &metaPacket, 1, sizeof( RawImportMeta ), meta );

        FILE* src = fopen( in, "rb" );
        enum { BufSize = 4*1024 };
        char buf[BufSize];
        while( size > 0 )
        {
            uint64_t toFill = std::min<uint64_t>( BufSize, size );
            fread( buf, 1, toFill, src );
            fwrite( buf, 1, toFill, data );
            size -= toFill;
        }
        fclose( src );
    }
    printf( "%i files processed.\n", list.size() );

    fclose( meta );
    fclose( data );

    return 0;
}
