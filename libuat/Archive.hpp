#ifndef __ARCHIVE_HPP__
#define __ARCHIVE_HPP__

#include <stdint.h>
#include <string>

#include "../common/FileMap.hpp"
#include "../common/HashSearch.hpp"
#include "../common/MetaView.hpp"
#include "../common/ZMessageView.hpp"

#include "ViewReference.hpp"

class Archive
{
public:
    Archive( const std::string& dir );

    const char* GetMessage( uint32_t idx );
    const char* GetMessage( const char* msgid );
    size_t NumberOfMessages() const { return m_mcnt; }

    ViewReference<uint32_t> GetTopLevel() const;
    size_t NumberOfTopLevel() const { return m_toplevel.Size(); }

private:
    ZMessageView m_mview;
    size_t m_mcnt;
    FileMap<uint32_t> m_toplevel;
    HashSearch m_midhash;
};

#endif
