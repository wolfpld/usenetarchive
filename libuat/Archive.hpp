#ifndef __ARCHIVE_HPP__
#define __ARCHIVE_HPP__

#include <map>
#include <stdint.h>
#include <string>
#include <vector>

#include "../common/FileMap.hpp"
#include "../common/HashSearch.hpp"
#include "../common/LexiconTypes.hpp"
#include "../common/MetaView.hpp"
#include "../common/ZMessageView.hpp"

#include "ViewReference.hpp"

struct SearchResult
{
    uint32_t postid;
    float rank;
    uint32_t timestamp;
};

class Archive
{
public:
    Archive( const std::string& dir );

    const char* GetMessage( uint32_t idx );
    const char* GetMessage( const char* msgid );
    size_t NumberOfMessages() const { return m_mcnt; }

    ViewReference<uint32_t> GetTopLevel() const;
    size_t NumberOfTopLevel() const { return m_toplevel.Size(); }

    int32_t GetParent( uint32_t idx ) const;
    int32_t GetParent( const char* msgid ) const;

    ViewReference<uint32_t> GetChildren( uint32_t idx ) const;
    ViewReference<uint32_t> GetChildren( const char* msgid ) const;

    uint32_t GetDate( uint32_t idx ) const;
    uint32_t GetDate( const char* msgid ) const;

    const char* GetFrom( uint32_t idx ) const;
    const char* GetFrom( const char* msgid ) const;

    const char* GetSubject( uint32_t idx ) const;
    const char* GetSubject( const char* msgid ) const;

    std::vector<SearchResult> Search( const char* query ) const;
    std::map<std::string, uint32_t> TimeChart() const;

private:
    ZMessageView m_mview;
    size_t m_mcnt;
    FileMap<uint32_t> m_toplevel;
    HashSearch m_midhash;
    MetaView<uint32_t, uint32_t> m_connectivity;
    MetaView<uint32_t, char> m_strings;
    FileMap<LexiconMetaPacket> m_lexmeta;
    FileMap<char> m_lexstr;
    FileMap<LexiconDataPacket> m_lexdata;
    FileMap<uint8_t> m_lexhit;
    HashSearch m_lexhash;
};

#endif
