#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s directory\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Destination directory doesn't exist.\n" );
        exit( 1 );
    }

    const auto base = std::string( argv[1] ) + "/";
    const auto listfn = base + "archives";
    if( !Exists( listfn ) )
    {
        fprintf( stderr, "Archive file list doesn't exist. Create %s with absolute paths to each archive in separate lines.\n", listfn.c_str() );
        exit( 1 );
    }

    std::vector<std::string> archives;

    // load archive list
    {
        FileMap<char> listfile( listfn );
        const char* begin = listfile;
        auto ptr = begin;
        auto size = listfile.DataSize();

        FILE* out = fopen( ( base + "archives.meta" ).c_str(), "wb" );

        auto end = ptr;
        while( size > 0 )
        {
            while( size > 0 && *end != '\r' && *end != '\n' )
            {
                end++;
                size--;
            }
            archives.emplace_back( ptr, end );

            if( !Exists( archives.back() ) )
            {
                fprintf( stderr, "Archive doesn't exist: %s\n", archives.back().c_str() );
                fclose( out );
                exit( 1 );
            }

            uint32_t tmp;
            tmp = ptr - begin;
            fwrite( &tmp, 1, sizeof( tmp ), out );
            tmp = end - begin;
            fwrite( &tmp, 1, sizeof( tmp ), out );

            while( size > 0 && ( *end == '\r' || *end == '\n' ) )
            {
                end++;
                size--;
            }
            ptr = end;
        }
        fclose( out );
    }

    return 0;
}
