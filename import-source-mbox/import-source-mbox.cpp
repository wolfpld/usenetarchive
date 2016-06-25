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

#include "../contrib/lz4/lz4.h"
#include "../contrib/lz4/lz4hc.h"
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

class ExpandingBuffer
{
public:
    ~ExpandingBuffer() { delete[] m_data; }

    char* Request( int size )
    {
        if( size > m_size )
        {
            delete[] m_data;
            m_data = new char[size];
            m_size = size;
        }
        return m_data;
    }

private:
    char* m_data = nullptr;
    int m_size = 0;
};

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
            if( !post.empty() )
            {
                int maxSize = LZ4_compressBound( post.size() );
                char* compressed = eb2.Request( maxSize );
                int csize = LZ4_compress_HC( post.data(), compressed, post.size(), maxSize, 16 );

                fwrite( compressed, 1, csize, data );

                RawImportMeta metaPacket = { offset, post.size(), csize };
                fwrite( &metaPacket, 1, sizeof( RawImportMeta ), meta );
                offset += csize;

                post.clear();

                if( ( idx & 0x3FF ) == 0 )
                {
                    printf( "%i\r", idx );
                    fflush( stdout );
                }
                idx++;
            }
        }
        else
        {
            post.append( line );
            post.append( "\n" );
        }
    }
    printf( "%i files processed.\n", idx );

    fclose( meta );
    fclose( data );

    return 0;
}
