#include <stdlib.h>
#include <mutex>

#include "../common/FileMap.hpp"
#include "../common/Filesystem.hpp"
#include "LockedFile.hpp"
#include "PersistentStorage.hpp"

static const char* LastOpenArchive = "lastopen";

static std::string GetSavePath()
{
#ifdef _WIN32
    std::string path( getenv( "APPDATA" ) );
    for( unsigned int i=0; i<path.size(); i++ )
    {
        if( path[i] == '\\' )
        {
            path[i] = '/';
        }
    }
    path += "/UsenetArchive/";
#else
    std::string path;
    const char* env = getenv( "XDG_CONFIG_HOME" );
    if( env && *env )
    {
        path = env;
        path += "/UsenetArchive/";
    }
    else
    {
        env = getenv( "HOME" );
        assert( env && *env );
        path = env;
        path += "/.config/UsenetArchive/";
    }
#endif
    return path;
}

PersistentStorage::PersistentStorage()
    : m_base( GetSavePath() )
{
}

PersistentStorage::~PersistentStorage()
{
}

void PersistentStorage::WriteLastOpenArchive( const char* archive )
{
    CreateDirStruct( m_base );
    LockedFile guard( ( m_base + LastOpenArchive ).c_str() );
    std::lock_guard<LockedFile> lg( guard );
    FILE* f = fopen( guard, "wb" );
    if( !f ) return;
    fwrite( archive, 1, strlen( archive ), f );
    fclose( f );
}

std::string PersistentStorage::ReadLastOpenArchive()
{
    std::string ret;
    const auto fn = m_base + LastOpenArchive;
    if( !Exists( fn ) ) return ret;
    LockedFile guard( fn.c_str() );
    std::lock_guard<LockedFile> lg( guard );
    FileMap<char> map( fn );
    ret.assign( map, map + map.Size() );
    return ret;
}
