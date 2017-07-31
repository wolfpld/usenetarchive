#ifndef __ARCHIVE_HPP__
#define __ARCHIVE_HPP__

#include <map>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

#include "../common/FileMap.hpp"
#include "../common/HashSearch.hpp"
#include "../common/LexiconTypes.hpp"
#include "../common/MetaView.hpp"
#include "../common/StringCompress.hpp"
#include "../common/ZMessageView.hpp"

#include "PackageAccess.hpp"
#include "ViewReference.hpp"

struct ScoreEntry;
class ExpandingBuffer;

class Archive
{
    friend class SearchEngine;

public:
    static Archive* Open( const std::string& fn );

    const char* GetMessage( uint32_t idx, ExpandingBuffer& eb ) { return idx >= m_mcnt ? nullptr : m_mview.GetMessage( idx, eb ); }
    const char* GetMessage( const char* msgid, ExpandingBuffer& eb ) { auto idx = m_midhash.Search( msgid ); return idx >= 0 ? GetMessage( idx, eb ) : nullptr; }
    size_t NumberOfMessages() const { return m_mcnt; }

    int GetMessageIndex( const char* msgid ) const { return m_midhash.Search( msgid ); }
    int GetMessageIndex( const char* msgid, XXH32_hash_t hash ) const { return m_midhash.Search( msgid, hash ); }
    const char* GetMessageId( uint32_t idx ) const { return m_middb[idx]; }

    ViewReference<uint32_t> GetTopLevel() const { return ViewReference<uint32_t> { m_toplevel, m_toplevel.DataSize() }; }
    size_t NumberOfTopLevel() const { return m_toplevel.DataSize(); }

    int32_t GetParent( uint32_t idx ) const { auto data = m_connectivity[idx]; return (int32_t)*(data+1); }
    int32_t GetParent( const char* msgid ) const { auto idx = m_midhash.Search( msgid ); return idx >= 0 ? GetParent( idx ) : -1; }

    ViewReference<uint32_t> GetChildren( uint32_t idx ) const { auto data = ( m_connectivity[idx] ) + 3; auto num = *data++; return ViewReference<uint32_t> { data, num }; }
    ViewReference<uint32_t> GetChildren( const char* msgid ) const { auto idx = m_midhash.Search( msgid ); return idx >= 0 ? GetChildren( idx ) : ViewReference<uint32_t> { nullptr, 0 }; }

    uint32_t GetTotalChildrenCount( uint32_t idx ) const { auto data = m_connectivity[idx]; return data[2]; }
    uint32_t GetTotalChildrenCount( const char* msgid ) const { auto idx = m_midhash.Search( msgid ); return idx >= 0 ? GetTotalChildrenCount( idx ) : 0; }

    uint32_t GetDate( uint32_t idx ) const { auto data = m_connectivity[idx]; return *data; }
    uint32_t GetDate( const char* msgid ) const { auto idx = m_midhash.Search( msgid ); return idx >= 0 ? GetDate( idx ) : 0; }

    const char* GetFrom( uint32_t idx ) const { return m_strings[idx*3]; }
    const char* GetFrom( const char* msgid ) const { auto idx = m_midhash.Search( msgid ); return idx >= 0 ? GetFrom( idx ) : nullptr; }

    const char* GetSubject( uint32_t idx ) const { return m_strings[idx*3+1]; }
    const char* GetSubject( const char* msgid ) const { auto idx = m_midhash.Search( msgid ); return idx >= 0 ? GetSubject( idx ) : nullptr; }

    const char* GetRealName( uint32_t idx ) const { return m_strings[idx*3+2]; }
    const char* GetRealName( const char* msgid ) const { auto idx = m_midhash.Search( msgid ); return idx >= 0 ? GetSubject( idx ) : nullptr; }

    int GetMessageScore( uint32_t idx, const std::vector<ScoreEntry>& scoreList ) const;

    std::map<std::string, uint32_t> TimeChart() const;

    std::pair<const char*, uint64_t> GetShortDescription() const { return std::make_pair( (const char*)m_descShort, m_descShort.Size() ); }
    std::pair<const char*, uint64_t> GetLongDescription() const { return std::make_pair( (const char*)m_descLong, m_descLong.Size() ); }
    std::pair<const char*, uint64_t> GetArchiveName() const { return std::make_pair( ( const char*)m_name, m_name.Size() ); }
    std::pair<const char*, uint64_t> GetPrefixList() const { return std::make_pair( ( const char*)m_prefix, m_prefix.Size() ); }

    size_t PackMsgId( const char* msgid, uint8_t* compressed ) const { return m_compress.Pack( msgid, compressed ); }
    size_t UnpackMsgId( const uint8_t* compressed, char* msgid ) const { return m_compress.Unpack( compressed, msgid ); }
    size_t RepackMsgId( const uint8_t* in, uint8_t* out, const StringCompress& other ) const { return m_compress.Repack( in, out, other ); }
    const StringCompress& GetCompress() const { return m_compress; }

private:
    Archive( const std::string& dir );
    Archive( const PackageAccess* pkg );

    std::unique_ptr<const PackageAccess> m_pkg;

    ZMessageView m_mview;
    const size_t m_mcnt;
    const FileMap<uint32_t> m_toplevel;
    const HashSearch m_midhash;
    const MetaView<uint32_t, char> m_middb;
    const MetaView<uint32_t, uint32_t> m_connectivity;
    const MetaView<uint32_t, char> m_strings;
    const FileMap<LexiconMetaPacket> m_lexmeta;
    const FileMap<char> m_lexstr;
    const FileMap<LexiconDataPacket> m_lexdata;
    const FileMap<uint8_t> m_lexhit;
    const HashSearch m_lexhash;
    const FileMap<char> m_descShort;
    const FileMap<char> m_descLong;
    const FileMap<char> m_name;
    const FileMap<char> m_prefix;
    const StringCompress m_compress;
    std::unique_ptr<MetaView<uint32_t, uint32_t>> m_lexdist;
};

#endif
