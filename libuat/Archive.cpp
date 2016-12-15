#include <algorithm>
#include <iterator>
#include <time.h>

#include "Archive.hpp"
#include "PackageAccess.hpp"
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
}

const char* Archive::GetMessage( uint32_t idx )
{
    return idx >= m_mcnt ? nullptr : m_mview[idx];
}

const char* Archive::GetMessage( const char* msgid )
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetMessage( idx ) : nullptr;
}

int Archive::GetMessageIndex( const char* msgid ) const
{
    return m_midhash.Search( msgid );
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

struct PostData
{
    uint32_t postid;
    uint8_t hitnum;
    uint8_t children;
    const uint8_t* hits;
};

struct MergedData
{
    uint32_t postid;
    float rank;
};

static float HitRank( const PostData& data )
{
    float rank = 0;
    auto ptr = data.hits;
    for( int i=0; i<data.hitnum; i++ )
    {
        auto test = LexiconHitRank( *ptr++ );
        if( test > rank ) rank = test;
    }
    return rank;
}

static float PostRank( const PostData& data )
{
    return ( float( data.children ) / LexiconChildMax ) * 0.9f + 0.1f;
}

SearchData Archive::Search( const char* query, int filter ) const
{
    std::vector<std::string> terms;
    split( query, std::back_inserter( terms ) );
    return Search( terms, filter );
}

SearchData Archive::Search( const std::vector<std::string>& terms, int filter ) const
{
    SearchData ret;

    std::vector<const char*> matched;
    std::vector<uint32_t> words;
    words.reserve( terms.size() );
    for( auto& v : terms )
    {
        auto res = m_lexhash.Search( v.c_str() );
        if( res >= 0 )
        {
            words.emplace_back( res );
            matched.emplace_back( m_lexstr + m_lexmeta[res].str );
        }
    }
    if( words.empty() ) return ret;

    std::vector<std::vector<PostData>> wdata;
    wdata.reserve( words.size() );
    for( auto& v : words )
    {
        auto meta = m_lexmeta[v];
        auto data = m_lexdata + ( meta.data / sizeof( LexiconDataPacket ) );

        wdata.emplace_back();
        auto& vec = wdata.back();
        vec.reserve( meta.dataSize );
        for( uint32_t i=0; i<meta.dataSize; i++ )
        {
            uint8_t children = data->postid >> LexiconChildShift;
            uint8_t hitnum = data->hitoffset >> LexiconHitShift;
            const uint8_t* hits;
            if( hitnum == 0 )
            {
                hits = m_lexhit + ( data->hitoffset & LexiconHitOffsetMask );
                hitnum = *hits++;
            }
            else
            {
                hits = (const uint8_t*)&data->hitoffset;
            }
            if( filter != T_All )
            {
                for( int j=0; j<hitnum; j++ )
                {
                    if( LexiconDecodeType( hits[j] ) == filter )
                    {
                        vec.emplace_back( PostData { data->postid & LexiconPostMask, hitnum, children, hits } );
                        break;
                    }
                }
            }
            else
            {
                vec.emplace_back( PostData { data->postid & LexiconPostMask, hitnum, children, hits } );
            }
            data++;
        }
    }

    std::vector<MergedData> merged;
    if( wdata.size() == 1 )
    {
        merged.reserve( wdata[0].size() );
        for( auto& v : wdata[0] )
        {
            merged.emplace_back( MergedData { v.postid, PostRank( v ) * HitRank( v ) } );
        }
    }
    else
    {
        std::vector<PostData*> list;
        list.reserve( words.size() );

        auto& vec = *wdata.begin();
        auto wsize = wdata.size();
        for( auto& post : vec )
        {
            list.clear();
            bool ok = true;
            for( size_t i=1; i<wsize; i++ )
            {
                auto& vtest = wdata[i];
                auto it = std::lower_bound( vtest.begin(), vtest.end(), post, [] ( const auto& l, const auto& r ) { return l.postid < r.postid; } );
                if( it == vtest.end() || it->postid != post.postid )
                {
                    ok = false;
                    break;
                }
                else
                {
                    list.emplace_back( &*it );
                }
            }
            if( ok )
            {
                list.emplace_back( &post );
                float rank = 0;
                for( auto& v : list )
                {
                    rank += HitRank( *v );
                }
                merged.emplace_back( MergedData { post.postid, rank * PostRank( post ) } );
            }
        }
    }
    if( merged.empty() ) return ret;

    std::sort( merged.begin(), merged.end(), []( const auto& l, const auto& r ) { return l.rank > r.rank; } );

    std::swap( ret.matched, matched );
    for( int i=0; i<merged.size(); i++ )
    {
        const auto postid = merged[i].postid;
        ret.results.emplace_back( SearchResult { postid, merged[i].rank, *m_connectivity[postid] } );
    }

    return ret;
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
