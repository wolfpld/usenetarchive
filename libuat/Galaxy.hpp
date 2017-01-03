#ifndef __GALAXY_HPP__
#define __GALAXY_HPP__

#include <memory>
#include <string>
#include <vector>

#include "../common/HashSearchBig.hpp"
#include "../common/MetaView.hpp"

#include "Archive.hpp"

class Galaxy
{
public:
    static Galaxy* Open( const std::string& fn );

    const std::vector<int>& GetAvailableArchives() const { return m_available; }
    std::string GetArchiveName( int idx ) const;
    const std::shared_ptr<Archive>& GetArchive( int idx );

    int GetActiveArchive() const { return m_active; }

private:
    Galaxy( const std::string& dir );

    std::string m_base;
    const MetaView<uint64_t, char> m_middb;
    const HashSearchBig m_midhash;
    const MetaView<uint32_t, char> m_archives;
    const MetaView<uint32_t, char> m_strings;
    const MetaView<uint32_t, uint32_t> m_midgr;

    std::vector<std::shared_ptr<Archive>> m_arch;
    std::vector<int> m_available;

    int m_active;
};

#endif
