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

struct PostData
{
    uint32_t postid;
    uint8_t hitnum;
    uint8_t children;
    const uint8_t* hits;
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

SearchData Archive::Search( const char* query, int flags, int filter ) const
{
    std::vector<std::string> terms;
    split( query, std::back_inserter( terms ) );
    return Search( terms, flags, filter );
}

static float GetWordDistance( const std::vector<PostData*>& list )
{
    const auto listsize = list.size();

    static thread_local std::vector<std::vector<uint8_t>> hits;
    const auto hs = hits.size();

    for( int i=0; i<listsize; i++ )
    {
        const auto& post = list[i];

        if( i < hs )
        {
            hits[i].clear();
        }
        else
        {
            hits.emplace_back();
        }
        auto& v = hits[i];

        for( int i=0; i<post->hitnum; i++ )
        {
            auto pos = LexiconHitPos( post->hits[i] );
            if( pos < LexiconHitPosMask[LexiconDecodeType( post->hits[i] )] )
            {
                v.emplace_back( post->hits[i] );
            }
        }
        std::sort( v.begin(), v.end(), []( const uint8_t l, const uint8_t r ) { return LexiconHitPos( l ) < LexiconHitPos( r ); } );
    }

    static thread_local std::vector<int> start[NUM_LEXICON_TYPES];
    static thread_local std::vector<std::vector<uint8_t>> hop[NUM_LEXICON_TYPES];

    const auto hops = hop[0].size();
    for( int i=0; i<NUM_LEXICON_TYPES; i++ )
    {
        start[i].clear();
        for( int j=0; j<listsize; j++ )
        {
            start[i].emplace_back( -1 );
        }
        if( listsize < hops )
        {
            for( int j=0; j<listsize; j++ )
            {
                hop[i][j].clear();
            }
        }
        else
        {
            for( int j=0; j<hops; j++ )
            {
                hop[i][j].clear();
            }
            for( int j=hops; j<listsize; j++ )
            {
                hop[i].emplace_back();
            }
        }
    }

    for( int i=0; i<listsize; i++ )
    {
        auto& post = hits[i];
        for( auto& hit : post )
        {
            auto type = LexiconDecodeType( hit );
            auto pos = LexiconHitPos( hit );
            if( start[type][i] == -1 )
            {
                start[type][i] = pos;
            }
            else
            {
                auto prev = hop[type][i].empty() ? start[type][i] : hop[type][i].back();
                auto diff = pos - prev;
                if( diff > 0 )
                {
                    hop[type][i].emplace_back( diff );
                }
            }
        }
    }

    int min = 0x7F;
    for( int t=0; t<NUM_LEXICON_TYPES; t++ )
    {
        for( int w1=0; w1<listsize - 1; w1++ )
        {
            if( start[t][w1] != -1 )
            {
                for( int w2=w1+1; w2<listsize; w2++ )
                {
                    if( start[t][w2] != -1 )
                    {
                        auto p1 = start[t][w1];
                        auto p2 = start[t][w2];

                        auto it1 = hop[t][w1].begin();
                        auto it2 = hop[t][w2].begin();

                        const auto end1 = hop[t][w1].end();
                        const auto end2 = hop[t][w2].end();

                        for(;;)
                        {
                            auto diff = p1 - p2;
                            auto ad = abs( diff );
                            if( ad < min )
                            {
                                if( ad < 2 )
                                {
                                    return 1;
                                }
                                else
                                {
                                    min = ad;
                                }
                            }
                            if( diff < 0 )
                            {
                                if( it1 != end1 )
                                {
                                    p1 += *it1++;
                                }
                                else
                                {
                                    break;
                                }
                            }
                            else
                            {
                                if( it2 != end2 )
                                {
                                    p2 += *it2++;
                                }
                                else
                                {
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    assert( min != 1 );
    return min;
}

SearchData Archive::Search( const std::vector<std::string>& terms, int flags, int filter ) const
{
    SearchData ret;

    if( flags & SF_FuzzySearch )
    {
        if( m_lexdist )
        {
            flags &= ~SF_RequireAllWords;
        }
        else
        {
            flags &= ~SF_FuzzySearch;
        }
    }

    std::unordered_set<uint32_t> wordset;
    std::vector<const char*> matched;
    std::vector<uint32_t> words;
    std::vector<float> wordMod;
    words.reserve( terms.size() );
    for( auto& v : terms )
    {
        auto res = m_lexhash.Search( v.c_str() );
        if( res >= 0 && wordset.find( res ) == wordset.end() )
        {
            words.emplace_back( res );
            wordset.emplace( res );
            wordMod.emplace_back( 1 );
            matched.emplace_back( m_lexstr + m_lexmeta[res].str );

            if( flags & SF_FuzzySearch )
            {
                auto ptr = (*m_lexdist)[res];
                const auto size = *ptr++;
                for( uint32_t i=0; i<size; i++ )
                {
                    const auto data = *ptr++;
                    const auto offset = data & 0x3FFFFFFF;
                    auto word = m_lexstr + offset;
                    auto res2 = m_lexhash.Search( word );
                    assert( res2 >= 0 );
                    if( wordset.find( res2 ) == wordset.end() )
                    {
                        words.emplace_back( res2 );
                        wordset.emplace( res2 );
                        const auto dist = data >> 30;
                        switch( dist )
                        {
                        case 1:
                            wordMod.emplace_back( 0.5f );
                            break;
                        case 2:
                            wordMod.emplace_back( 0.25f );
                            break;
                        default:    // 3
                            wordMod.emplace_back( 0.125f );
                            break;
                        }
                        matched.emplace_back( word );
                    }
                }
            }
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

    std::vector<SearchResult> result;

    const auto wsize = wdata.size();
    if( wsize == 1 )
    {
        result.reserve( wdata[0].size() );
        for( auto& v : wdata[0] )
        {
            result.emplace_back( SearchResult { v.postid, PostRank( v ) * HitRank( v ) } );
        }
    }
    else if( flags & SF_RequireAllWords )
    {
        std::vector<PostData*> list;
        list.reserve( words.size() );

        auto& vec = *wdata.begin();
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
                if( flags & SF_AdjacentWords )
                {
                    rank /= GetWordDistance( list );
                }
                result.emplace_back( SearchResult { post.postid, rank * PostRank( post ) } );
            }
        }
    }
    else
    {
        struct Posts
        {
            uint32_t word;
            PostData* data;
        };
        std::unordered_map<uint32_t, std::vector<Posts>> posts;
        for( uint32_t word = 0; word < wsize; word++ )
        {
            for( auto& post : wdata[word] )
            {
                auto pidx = post.postid;
                auto it = posts.find( pidx );
                if( it == posts.end() )
                {
                    posts.emplace( pidx, std::vector<Posts>( { Posts { word, &post } } ) );
                }
                else
                {
                    it->second.emplace_back( Posts { word, &post } );
                }
            }
        }
        std::vector<PostData*> list;
        for( auto& entry : posts )
        {
            float rank = 0;
            for( auto& v : entry.second )
            {
                rank += HitRank( *v.data ) * wordMod[v.word];
            }
            if( flags & SF_AdjacentWords )
            {
                list.clear();
                for( auto& v : entry.second )
                {
                    list.emplace_back( v.data );
                }
                rank /= GetWordDistance( list );
            }
            result.emplace_back( SearchResult { entry.first, rank * PostRank( *entry.second[0].data ) } );
        }
    }
    if( result.empty() ) return ret;

    std::sort( result.begin(), result.end(), []( const auto& l, const auto& r ) { return l.rank > r.rank; } );

    std::swap( ret.matched, matched );
    std::swap( ret.results, result );

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
