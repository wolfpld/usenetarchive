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

    uint32_t Version() const { return m_version; }

private:
    PackageAccess( const std::string& fn, uint32_t version );

    FileMap<char> m_file;
    uint64_t m_sizes[PackageFiles];
    uint64_t m_offsets[PackageFiles];
    uint32_t m_version;
};

#endif
