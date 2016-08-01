#ifndef __PACKAGEACCESS_HPP__
#define __PACKAGEACCESS_HPP__

#include <string>
#include <stdint.h>

#include "../common/FileMap.hpp"
#include "../common/Package.hpp"

class PackageAccess
{
public:
    static PackageAccess* Open( const std::string& fn );

    FileMapPtrs Get( PackageFile::type fn ) const;

private:
    PackageAccess( const std::string& fn );

    FileMap<char> m_file;
    uint64_t m_sizes[PackageFiles];
    uint64_t m_offsets[PackageFiles];
};

#endif
