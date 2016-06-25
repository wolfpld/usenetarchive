#ifndef __RAWIMPORTMETA_HPP__
#define __RAWIMPORTMETA_HPP__

#include <stdint.h>

struct RawImportMeta
{
    uint64_t offset;
    uint32_t size;
    uint32_t compressedSize;
};

#endif
