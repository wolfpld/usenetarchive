#ifndef __GALAXY_HPP__
#define __GALAXY_HPP__

#include <memory>
#include <string>
#include <vector>

#include "../common/HashSearchBig.hpp"
#include "../common/MetaView.hpp"

#include "Archive.hpp"
#include "ViewReference.hpp"

class Galaxy
{
public:
    static Galaxy* Open( const std::string& fn );

    size_t GetNumberOfArchives() const { return m_arch.size(); }
    const std::vector<int>& GetAvailableArchives() const { return m_available; }
    const std::shared_ptr<Archive>& GetArchive( int idx );

    bool IsArchiveAvailable( int idx ) const;
    std::string GetArchiveFilename( int idx ) const;
    const char* GetArchiveName( int idx ) const;
    const char* GetArchiveDescription( int idx ) const;
    int NumberOfMessages( int idx ) const;
    int NumberOfTopLevel( int idx ) const;

    int GetActiveArchive() const { return m_active; }

    int GetMessageIndex( const char* msgid ) const;

    int GetNumberOfGroups( uint32_t idx ) const;
    int GetNumberOfGroups( const char* msgid ) const;

    bool AreChildrenSame( uint32_t idx, const char* msgid ) const;
    bool AreChildrenSame( const char* msgid ) const;

    bool AreParentsSame( uint32_t idx, const char* msgid ) const;
    bool AreParentsSame( const char* msgid ) const;

    ViewReference<uint32_t> GetGroups( const char* msgid ) const;

    int ParentDepth( const char* msgid, uint32_t arch ) const;
    int NumberOfChildren( const char* msgid, uint32_t arch ) const;
    int TotalNumberOfChildren( const char* msgid, uint32_t arch ) const;

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
