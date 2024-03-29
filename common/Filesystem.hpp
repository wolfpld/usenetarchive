#ifndef __FILESYSTEM_HPP__
#define __FILESYSTEM_HPP__

#include <stdint.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <vector>

#ifdef CopyFile
#  undef CopyFile
#endif

bool CreateDirStruct( const std::string& path );
std::vector<std::string> ListDirectory( const std::string& path );
void CopyFile( const std::string& from, const std::string& to );

void CopyCommonFiles( const std::string& source, const std::string& target );

#ifdef _MSC_VER
#  define stat64 _stat64
#endif
#if defined __CYGWIN__ || defined __APPLE__
#  define stat64 stat
#endif

#ifndef S_ISREG
#  define S_ISREG(m) ( ( (m) & S_IFMT ) == S_IFREG )
#endif

static inline bool Exists( const std::string& path )
{
    struct stat64 buf;
    return stat64( path.c_str(), &buf ) == 0;
}

static inline bool IsFile( const std::string& path )
{
    struct stat64 buf;
    if( stat64( path.c_str(), &buf ) == 0 )
    {
        return S_ISREG( buf.st_mode );
    }
    else
    {
        return false;
    }
}

static inline uint64_t GetFileSize( const char* path )
{
    struct stat64 buf;
    if( stat64( path, &buf ) == 0 )
    {
        return buf.st_size;
    }
    else
    {
        return 0;
    }
}

static inline int64_t GetFileMTime( const char* path )
{
    struct stat64 buf;
    if( stat64( path, &buf ) == 0 )
    {
        return (int64_t)buf.st_mtime;
    }
    else
    {
        return 0;
    }
}

#endif
