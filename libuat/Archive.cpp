#include <algorithm>
#include <iterator>
#include <time.h>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "Archive.hpp"
#include "PackageAccess.hpp"
#include "Score.hpp"

#include "../common/Filesystem.hpp"
#include "../common/String.hpp"
#include "../common/Package.hpp"

Archive* Archive::Open( const std::string& fn )
{
    if( !Exists( fn ) ) return nullptr;
    if( IsFile( fn ) )
    {
        auto pkg = PackageAccess::Open( fn );
        if( !pkg ) return nullptr;
        return new Archive( pkg );
    }
    else
    {
        auto base = fn + "/";
        if( !Exists( base + "zmeta" ) || !Exists( base + "zdata" ) || !Exists( base + "zdict" ) ||
            !Exists( base + "toplevel" ) || !Exists( base + "connmeta" ) || !Exists( base + "conndata" ) ||
            !Exists( base + "middata" ) || !Exists( base + "midhash" ) || !Exists( base + "midhashdata" ) || !Exists( base + "midmeta" ) ||
            !Exists( base + "strmeta" ) || !Exists( base + "strings" ) || !Exists( base + "lexmeta" ) ||
            !Exists( base + "lexstr" ) || !Exists( base + "lexdata" ) || !Exists( base + "lexhit" ) ||
            !Exists( base + "lexstr" ) || !Exists( base + "lexhash" ) || !Exists( base + "lexhashdata" ) )
        {
            return nullptr;
        }
        else
        {
            return new Archive( base );
        }
    }
}

Archive::Archive( const std::string& dir )
    : m_mview( dir + "zmeta", dir + "zdata", dir + "zdict" )
    , m_mcnt( m_mview.Size() )
    , m_toplevel( dir + "toplevel" )
    , m_midhash( dir + "middata", dir + "midhash", dir + "midhashdata" )
    , m_middb( dir + "midmeta", dir + "middata" )
    , m_connectivity( dir + "connmeta", dir + "conndata" )
    , m_strings( dir + "strmeta", dir + "strings" )
    , m_lexmeta( dir + "lexmeta" )
    , m_lexstr( dir + "lexstr" )
    , m_lexdata( dir + "lexdata" )
    , m_lexhit( dir + "lexhit" )
    , m_lexhash( dir + "lexstr", dir + "lexhash", dir + "lexhashdata" )
    , m_descShort( dir + "desc_short", true )
    , m_descLong( dir + "desc_long", true )
    , m_name( dir + "name", true )
{
    if( Exists( dir + "lexdist" ) && Exists( dir + "lexdistmeta" ) )
    {
        m_lexdist = std::make_unique<MetaView<uint32_t, uint32_t>>( dir + "lexdistmeta", dir + "lexdist" );
    }
}

Archive::Archive( const PackageAccess* pkg )
    : m_pkg( pkg )
    , m_mview( pkg->Get( PackageFile::zmeta ), pkg->Get( PackageFile::zdata ), pkg->Get( PackageFile::zdict ) )
    , m_mcnt( m_mview.Size() )
    , m_toplevel( pkg->Get( PackageFile::toplevel ) )
    , m_midhash( pkg->Get( PackageFile::middata ), pkg->Get( PackageFile::midhash ), pkg->Get( PackageFile::midhashdata ) )
    , m_middb( pkg->Get( PackageFile::midmeta ), pkg->Get( PackageFile::middata ) )
    , m_connectivity( pkg->Get( PackageFile::connmeta ), pkg->Get( PackageFile::conndata ) )
    , m_strings( pkg->Get( PackageFile::strmeta ), pkg->Get( PackageFile::strings ) )
    , m_lexmeta( pkg->Get( PackageFile::lexmeta ) )
    , m_lexstr( pkg->Get( PackageFile::lexstr ) )
    , m_lexdata( pkg->Get( PackageFile::lexdata ) )
    , m_lexhit( pkg->Get( PackageFile::lexhit ) )
    , m_lexhash( pkg->Get( PackageFile::lexstr ), pkg->Get( PackageFile::lexhash ), pkg->Get( PackageFile::lexhashdata ) )
    , m_descShort( pkg->Get( PackageFile::desc_short ) )
    , m_descLong( pkg->Get( PackageFile::desc_long ) )
    , m_name( pkg->Get( PackageFile::name ) )
{
    const auto lexdist = pkg->Get( PackageFile::lexdist );
    const auto lexdistmeta = pkg->Get( PackageFile::lexdistmeta );
    if( lexdist.size > 0 && lexdistmeta.size > 0 )
    {
        m_lexdist = std::make_unique<MetaView<uint32_t, uint32_t>>( lexdistmeta, lexdist );
    }
}

const char* Archive::GetMessage( uint32_t idx, ExpandingBuffer& eb )
{
    return idx >= m_mcnt ? nullptr : m_mview.GetMessage( idx, eb );
}

const char* Archive::GetMessage( const char* msgid, ExpandingBuffer& eb )
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetMessage( idx, eb ) : nullptr;
}

int Archive::GetMessageIndex( const char* msgid ) const
{
    return m_midhash.Search( msgid );
}

int Archive::GetMessageIndex( const char* msgid, XXH32_hash_t hash ) const
{
    return m_midhash.Search( msgid, hash );
}

const char* Archive::GetMessageId( uint32_t idx ) const
{
    return m_middb[idx];
}

ViewReference<uint32_t> Archive::GetTopLevel() const
{
    return ViewReference<uint32_t> { m_toplevel, m_toplevel.DataSize() };
}

int32_t Archive::GetParent( uint32_t idx ) const
{
    auto data = m_connectivity[idx];
    data++;
    return (int32_t)*data;
}

int32_t Archive::GetParent( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetParent( idx ) : -1;
}

ViewReference<uint32_t> Archive::GetChildren( uint32_t idx ) const
{
    auto data = m_connectivity[idx];
    data += 3;
    auto num = *data++;
    return ViewReference<uint32_t> { data, num };
}

ViewReference<uint32_t> Archive::GetChildren( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetChildren( idx ) : ViewReference<uint32_t> { nullptr, 0 };
}

uint32_t Archive::GetTotalChildrenCount( uint32_t idx ) const
{
    auto data = m_connectivity[idx];
    return data[2];
}

uint32_t Archive::GetTotalChildrenCount( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetTotalChildrenCount( idx ) : 0;
}

uint32_t Archive::GetDate( uint32_t idx ) const
{
    auto data = m_connectivity[idx];
    return *data;
}

uint32_t Archive::GetDate( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetDate( idx ) : 0;
}

const char* Archive::GetFrom( uint32_t idx ) const
{
    return m_strings[idx*3];
}

const char* Archive::GetFrom( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetFrom( idx ) : nullptr;
}

const char* Archive::GetSubject( uint32_t idx ) const
{
    return m_strings[idx*3+1];
}

const char* Archive::GetSubject( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetSubject( idx ) : nullptr;
}

const char* Archive::GetRealName( uint32_t idx ) const
{
    return m_strings[idx*3+2];
}

const char* Archive::GetRealName( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetSubject( idx ) : nullptr;
}

static bool MatchStrings( const std::string& s1, const char* s2, bool exact, bool ignoreCase )
{
    if( exact )
    {
        if( ignoreCase )
        {
            auto l1 = s1.size();
            auto l2 = strlen( s2 );
            return l1 == l2 && strnicmpl( s2, s1.c_str(), l1 ) == 0;
        }
        else
        {
            return strcmp( s1.c_str(), s2 ) == 0;
        }
    }
    else
    {
        std::regex r( s1.c_str(), s1.size(), std::regex::flag_type( std::regex_constants::ECMAScript | ( ignoreCase ? std::regex_constants::icase : 0 ) ) );
        return std::regex_search( s2, r );
    }
}

int Archive::GetMessageScore( uint32_t idx, const std::vector<ScoreEntry>& scoreList ) const
{
    int score = 0;
    for( auto& v : scoreList )
    {
        bool match = false;
        switch( v.field )
        {
        case SF_RealName:
            match = MatchStrings( v.match, GetRealName( idx ), v.exact != 0, v.ignoreCase != 0 );
            break;
        case SF_Subject:
            match = MatchStrings( v.match, GetSubject( idx ), v.exact != 0, v.ignoreCase != 0 );
            break;
        case SF_From:
            match = MatchStrings( v.match, GetFrom( idx ), v.exact != 0, v.ignoreCase != 0 );
            break;
        default:
            assert( false );
            break;
        }
        if( match )
        {
            score += v.score;
        }
    }
    return score;
}

std::map<std::string, uint32_t> Archive::TimeChart() const
{
    std::map<std::string, uint32_t> ret;

    const auto size = m_connectivity.Size();
    for( size_t i=0; i<size; i++ )
    {
        const auto epoch = time_t( *m_connectivity[i] );
        if( epoch == 0 ) continue;
        char buf[16];
        strftime( buf, 16, "%Y%m", gmtime( &epoch ) );
        ret[buf]++;
    }

    return ret;
}

std::pair<const char*, uint64_t> Archive::GetShortDescription() const
{
    return std::make_pair( (const char*)m_descShort, m_descShort.Size() );
}

std::pair<const char*, uint64_t> Archive::GetLongDescription() const
{
    return std::make_pair( (const char*)m_descLong, m_descLong.Size() );
}

std::pair<const char*, uint64_t> Archive::GetArchiveName() const
{
    return std::make_pair( ( const char*)m_name, m_name.Size() );
}
