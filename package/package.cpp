#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <vector>

#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/Package.hpp"

int main( int argc, char** argv )
{
    if( argc < 3 )
    {
        fprintf( stderr, "USAGE: %s [-x] source destination\nParams:\n", argv[0] );
        fprintf( stderr, "  -x            extract\n" );
        exit( 1 );
    }

    bool extract = strcmp( argv[1], "-x" ) == 0;
    if( extract )
    {
        argv++;
    }

    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Source doesn't exist.\n" );
        exit( 1 );
    }
    if( Exists( argv[2] ) )
    {
        fprintf( stderr, "Destination exist.\n" );
        exit( 1 );
    }

    std::vector<FileMap<char>> ptrs;

    if( extract )
    {
        FILE* fin = fopen( argv[1], "rb" );
        uint64_t offset = 0;
        char tmp[PackageHeaderSize];
        offset += fread( tmp, 1, PackageHeaderSize, fin );
        if( memcmp( tmp, PackageHeader, PackageMagicSize ) != 0 )
        {
            fprintf( stderr, "Source file is not an Usenet archive.\n" );
            exit( 1 );
        }
        const auto version = tmp[PackageMagicSize];
        if( version > PackageVersion )
        {
            fprintf( stderr, "Archive version %i is not supported. Update your tools.\n", tmp[PackageMagicSize] );
        }

        int numfiles = PackageFiles;
        if( version < 2 )
        {
            numfiles -= AdditionalFilesV2;
        }
        if( version < 1 )
        {
            numfiles -= AdditionalFilesV1;
        }

        uint64_t sizes[PackageFiles];
        for( int i=0; i<numfiles; i++ )
        {
            offset += fread( sizes+i, 1, sizeof( uint64_t ), fin );
        }

        std::string base( argv[2] );
        CreateDirStruct( base );
        base.append( "/" );

        for( int i=0; i<numfiles; i++ )
        {
            if( sizes[i] == 0 ) continue;
            FILE* fout = fopen( ( base + PackageContents[i].filename ).c_str(), "wb" );

            enum { DataBlock = 64 * 1024 };
            char data[DataBlock];
            uint64_t left = sizes[i];
            while( left > 0 )
            {
                uint64_t todo = std::min<uint64_t>( left, DataBlock );
                offset += fread( data, 1, todo, fin );
                fwrite( data, 1, todo, fout );
                left -= todo;
            }
            auto align = PackageAlign( offset );
            offset += fread( tmp, 1, align - offset, fin );
        }
    }
    else
    {
        const char zero[8] = {};
        std::string base( argv[1] );
        base.append( "/" );

        for( int i=0; i<PackageFiles; i++ )
        {
            ptrs.emplace_back( base + PackageContents[i].filename, PackageContents[i].optional );
        }

        uint64_t offset = 0;
        FILE* f = fopen( argv[2], "wb" );
        offset += fwrite( PackageHeader, 1, PackageHeaderSize, f );
        for( int i=0; i<PackageFiles; i++ )
        {
            uint64_t size = ptrs[i].Size();
            offset += fwrite( &size, 1, sizeof( size ), f );
        }
        assert( PackageAlign( offset ) == offset );
        for( int i=0; i<PackageFiles; i++ )
        {
            offset += fwrite( (const char*)ptrs[i], 1, ptrs[i].Size(), f );
            auto align = PackageAlign( offset );
            offset += fwrite( zero, 1, align - offset, f );
        }
        fclose( f );
    }

    return 0;
}
