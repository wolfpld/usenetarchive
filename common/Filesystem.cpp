#include "Filesystem.hpp"

#ifdef _WIN32
#  include <direct.h>
#  include <windows.h>
#else
#  include <dirent.h>
#endif

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
