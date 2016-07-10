#ifndef __ARCHIVE_HPP__
#define __ARCHIVE_HPP__

#include <stdint.h>
#include <string>

#include "../common/FileMap.hpp"
#include "../common/MetaView.hpp"
#include "../common/ZMessageView.hpp"

#include "ViewReference.hpp"

class Archive
{
public:
    Archive( const std::string& dir );

    const char* GetMessage( uint32_t idx );
    size_t NumberOfMessages() const { return m_mcnt; }

    ViewReference<uint32_t> GetTopLevel() const;

private:
    ZMessageView m_mview;
    size_t m_mcnt;
    FileMap<uint32_t> m_toplevel;
};

#endif
