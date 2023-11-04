#ifndef __VIEWREFERENCE_HPP__
#define __VIEWREFERENCE_HPP__

#include <stdint.h>

template<typename T>
struct ViewReference
{
    const T* ptr;
    uint64_t size;
};

#endif
