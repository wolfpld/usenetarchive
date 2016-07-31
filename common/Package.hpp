#ifndef __PACKAGE_HPP__
#define __PACKAGE_HPP__

struct PackMeta
{
    const char* filename;
    bool optional;
};

static const PackMeta PackageContents[] = {
    { "desc_short", true },
    { "desc_long", true },
    { "conndata", false },
    { "connmeta", false },
    { "lexdata", false },
    { "lexmeta", false },
    { "lexhash", false },
    { "lexhashdata", false },
    { "lexhit", false },
    { "lexstr", false },
    { "middata", false },
    { "midmeta", false },
    { "midhash", false },
    { "midhashdata", false },
    { "strings", false },
    { "strmeta", false },
    { "toplevel", false },
    { "zdata", false },
    { "zmeta", false },
    { "zdict", false }
};

enum { PackageFiles = sizeof( PackageContents ) / sizeof( PackMeta ) };

enum : char { PackageVersion = 0 };
enum { PackageHeaderSize = 8 };
static const char PackageHeader[PackageHeaderSize] = { '\0', 'U', 's', 'e', 'n', 'e', 't', PackageVersion };

static inline uint64_t PackageAlign( uint64_t offset ) { return ( ( offset + 7 ) / 8 ) * 8; }

#endif
