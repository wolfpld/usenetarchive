#ifndef __HEADERBAR_HPP__
#define __HEADERBAR_HPP__

#include <stdint.h>
#include <utility>

#include "View.hpp"

class HeaderBar : public View
{
public:
    HeaderBar( const std::pair<const char*, uint64_t>& archive, const char* desc, const char* fn );

    void Resize() const;

    void Change( const std::pair<const char*, uint64_t>& archive, const char* desc, const char* fn );

private:
    void Redraw() const;

    const char* m_archive;
    const char* m_desc;
    uint64_t m_archiveLen;
};

#endif
