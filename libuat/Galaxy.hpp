#ifndef __GALAXY_HPP__
#define __GALAXY_HPP__

#include <assert.h>
#include <memory>
#include <string>
#include <vector>

#include "../common/FileMap.hpp"
#include "../common/HashSearchBig.hpp"
#include "../common/MetaView.hpp"
#include "../common/StringCompress.hpp"

#include "Archive.hpp"
#include "ViewReference.hpp"

class Galaxy
{
public:
    static Galaxy* Open( const std::string& fn );

    size_t GetNumberOfArchives() const { return m_arch.size(); }
    const std::vector<int>& GetAvailableArchives() const { return m_available; }
    const std::shared_ptr<Archive>& GetArchive( int idx, bool change = true );

    bool IsArchiveAvailable( int idx ) const { return bool( m_arch[idx] ); }
    std::string GetArchiveFilename( int idx ) const { return std::string( m_archives[idx*2], m_archives[idx*2+1] ); }
    const char* GetArchiveName( int idx ) const { return m_strings[idx*2]; }
    const char* GetArchiveDescription( int idx ) const { return m_strings[idx*2+1]; }
    int NumberOfMessages( int idx ) const { return m_arch[idx]->NumberOfMessages(); }
    int NumberOfTopLevel( int idx ) const { return m_arch[idx]->NumberOfTopLevel(); }

    int GetActiveArchive() const { return m_active; }

    int GetMessageIndex( const uint8_t* msgid ) const { return m_midhash.Search( msgid ); }
    const char* GetMessageId( uint32_t idx ) const { return m_middb[idx]; }

    size_t PackMsgId( const char* msgid, uint8_t* compressed ) const { return m_compress.Pack( msgid, compressed ); }
    size_t UnpackMsgId( const uint8_t* compressed, char* msgid ) const { return m_compress.Unpack( compressed, msgid ); }

    int GetNumberOfGroups( uint32_t idx ) const { if( idx == -1 ) return 0; return *m_midgr[idx]; }
    bool AreChildrenSame( uint32_t idx, const char* msgid ) const;
    bool AreParentsSame( uint32_t idx, const char* msgid ) const;
    ViewReference<uint32_t> GetGroups( uint32_t idx ) const { assert( idx != -1 ); auto ptr = m_midgr[idx]; auto num = *ptr++; return ViewReference<uint32_t> { ptr, num }; }
    int32_t GetIndirectIndex( uint32_t idx ) const;

    ViewReference<uint32_t> GetIndirectParents( uint32_t indirect_idx ) const { assert( indirect_idx != -1 ); auto ptr = m_indirect[indirect_idx*2]; auto num = *ptr++; return ViewReference<uint32_t> { ptr, num }; }
    ViewReference<uint32_t> GetIndirectChildren( uint32_t indirect_idx ) const { assert( indirect_idx != -1 ); auto ptr = m_indirect[indirect_idx*2+1]; auto num = *ptr++; return ViewReference<uint32_t> { ptr, num }; }

    int ParentDepth( const char* msgid, uint32_t arch ) const;
    int NumberOfChildren( const char* msgid, uint32_t arch ) const { return m_arch[arch]->GetChildren( msgid ).size; }
    int TotalNumberOfChildren( const char* msgid, uint32_t arch ) const { return m_arch[arch]->GetTotalChildrenCount( msgid ); }

private:
    Galaxy( const std::string& dir );

    std::string m_base;
    const MetaView<uint64_t, char> m_middb;
    const HashSearchBig m_midhash;
    const MetaView<uint32_t, char> m_archives;
    const MetaView<uint32_t, char> m_strings;
    const MetaView<uint32_t, uint32_t> m_midgr;
    const MetaView<uint32_t, uint32_t> m_indirect;
    const FileMap<uint64_t> m_indirectDense;
    const StringCompress m_compress;

    std::vector<std::shared_ptr<Archive>> m_arch;
    std::vector<int> m_available;

    int m_active;
};

#endif
