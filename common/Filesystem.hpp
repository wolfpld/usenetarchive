#ifndef __FILESYSTEM_HPP__
#define __FILESYSTEM_HPP__

#include <string>
#include <sys/types.h>
#include <sys/stat.h>

bool CreateDirStruct( const std::string& path );

#ifdef _WIN32
#  define stat64 _stat64
#endif

static inline bool Exists( const std::string& path )
{
    struct stat64 buf;
    return stat64( path.c_str(), &buf ) == 0;
}

static inline uint64_t GetFileSize( const char* path )
{
    struct stat64 buf;
    stat64( path, &buf );
    return buf.st_size;
}

#endif
