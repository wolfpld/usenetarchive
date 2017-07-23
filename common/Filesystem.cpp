#include <assert.h>
#include "Filesystem.hpp"

#ifdef _WIN32
#  include <direct.h>
#  include <windows.h>
#else
#  include <dirent.h>
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include <string.h>

bool CreateDirStruct( const std::string& path )
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

std::vector<std::string> ListDirectory( const std::string& path )
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
            char tmp[1024];
            sprintf( tmp, "%s/%s", path.c_str(), ent->d_name );
            struct stat stat;
            lstat( tmp, &stat );
            if( S_ISDIR( stat.st_mode ) )
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

#ifdef CopyFile
#  undef CopyFile
#endif

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

void CopyCommonFiles( const std::string& source, const std::string& target )
{
    if( Exists( source + "name" ) ) CopyFile( source + "name", target + "name" );
    if( Exists( source + "desc_long" ) ) CopyFile( source + "desc_long", target + "desc_long" );
    if( Exists( source + "desc_short" ) ) CopyFile( source + "desc_short", target + "desc_short" );
    if( Exists( source + "prefix" ) ) CopyFile( source + "prefix", target + "prefix" );
}
