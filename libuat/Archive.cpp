#include <algorithm>
#include <iterator>
#include <time.h>
#include <regex>
#include <vector>

#include "Archive.hpp"
#include "PackageAccess.hpp"
#include "Score.hpp"

#include "../common/Filesystem.hpp"
#include "../common/String.hpp"
#include "../common/Package.hpp"

Archive* Archive::Open( const std::string& fn )
{
    if( IsFile( fn ) )
    {
        auto pkg = PackageAccess::Open( fn );
        if( !pkg ) return nullptr;
        if( pkg->Version() < PackageVersion ) return nullptr;
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
    , m_prefix( dir + "prefix", true )
    , m_compress( dir + "msgid.codebook" )
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
    , m_prefix( pkg->Get( PackageFile::prefix ) )
    , m_compress( pkg->Get( PackageFile::codebook ) )
{
    const auto lexdist = pkg->Get( PackageFile::lexdist );
    const auto lexdistmeta = pkg->Get( PackageFile::lexdistmeta );
    if( lexdist.size > 0 && lexdistmeta.size > 0 )
    {
        m_lexdist = std::make_unique<MetaView<uint32_t, uint32_t>>( lexdistmeta, lexdist );
    }
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
