#ifndef __FILESYSTEM_HPP__
#define __FILESYSTEM_HPP__

#include <string>
#include <sys/types.h>
#include <sys/stat.h>

bool CreateDirStruct( const std::string& path );

static inline bool Exists( const std::string& path )
{
    struct stat buf;
    return stat( path.c_str(), &buf ) == 0;
}

static inline uint64_t GetFileSize( const char* path )
{
    struct stat buf;
    stat( path, &buf );
    return buf.st_size;
}

#endif
