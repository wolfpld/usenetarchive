#include <stdio.h>
#include <string.h>

#include "PackageAccess.hpp"

PackageAccess* PackageAccess::Open( const std::string& fn )
{
    char version;
    FILE* f = fopen( fn.c_str(), "rb" );
    if( !f ) return nullptr;
    char tmp[PackageHeaderSize];
    if( fread( tmp, 1, PackageHeaderSize, f ) != PackageHeaderSize ) goto err;
    if( memcmp( tmp, PackageHeader, PackageMagicSize ) != 0 ) goto err;
    version = tmp[PackageMagicSize];
    if( version > PackageVersion ) goto err;
    fclose( f );
    return new PackageAccess( fn, version );

err:
    fclose( f );
    return nullptr;
}

PackageAccess::PackageAccess( const std::string& fn, uint32_t version )
    : m_file( fn )
    , m_version( version )
{
    memcpy( m_sizes, m_file + PackageHeaderSize, PackageFiles * sizeof( uint64_t ) );
    uint64_t offset = PackageHeaderSize + PackageFiles * sizeof( uint64_t );
    for( int i=0; i<PackageFiles; i++ )
    {
        m_offsets[i] = offset;
        offset = PackageAlign( offset + m_sizes[i] );
    }
}

FileMapPtrs PackageAccess::Get( PackageFile::type fn ) const
{
    return FileMapPtrs { m_file + m_offsets[fn], m_sizes[fn] };
}
