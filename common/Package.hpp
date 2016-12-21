#ifndef __PACKAGE_HPP__
#define __PACKAGE_HPP__

#include <stdint.h>

struct PackMeta
{
    const char* filename;
    bool optional;
};

static const PackMeta PackageContents[] = {
    { "name", true },
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
    { "zdict", false },
    { "lexdist", true },
    { "lexdistmeta", true }
};

struct PackageFile
{
    enum type {
        name,
        desc_short,
        desc_long,
        conndata,
        connmeta,
        lexdata,
        lexmeta,
        lexhash,
        lexhashdata,
        lexhit,
        lexstr,
        middata,
        midmeta,
        midhash,
        midhashdata,
        strings,
        strmeta,
        toplevel,
        zdata,
        zmeta,
        zdict,
        lexdist,
        lexdistmeta
    };
};

enum { PackageFiles = sizeof( PackageContents ) / sizeof( PackMeta ) };
enum { AdditionalFilesV1 = 2 };

enum : char { PackageVersion = 1 };
enum { PackageHeaderSize = 8 };
enum { PackageMagicSize = PackageHeaderSize - 1 };
static const char PackageHeader[PackageHeaderSize] = { '\0', 'U', 's', 'e', 'n', 'e', 't', PackageVersion };

static inline uint64_t PackageAlign( uint64_t offset ) { return ( ( offset + 7 ) / 8 ) * 8; }

#endif
