#ifndef __ARCHIVE_HPP__
#define __ARCHIVE_HPP__

#include <stdint.h>
#include <string>

#include "../common/MetaView.hpp"
#include "../common/ZMessageView.hpp"

class Archive
{
public:
    Archive( const std::string& dir );

    const char* GetMessage( uint32_t idx );
    size_t NumberOfMessages() const { return m_mcnt; }

private:
    ZMessageView m_mview;
    size_t m_mcnt;
};

#endif
