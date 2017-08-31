#include <assert.h>
#include <limits>
#include <unordered_set>

#include "Archive.hpp"
#include "SearchEngine.hpp"

#include "../common/Slab.hpp"
#include "../common/String.hpp"

enum { SlabSize = 128*1024*1024 };
static thread_local Slab<SlabSize> slab;

enum WordFlags
{
    WF_None     = 0,
    WF_Must     = 1 << 0,
    WF_Cant     = 1 << 1,
    WF_From     = 1 << 2,
    WF_Subject  = 1 << 3,
};

struct PostData
{
    uint32_t postid;
    uint8_t hitnum;
    uint8_t children;
    const uint8_t* hits;
};


SearchEngine::SearchEngine( const Archive& archive )
    : m_archive( archive )
{
}

SearchData SearchEngine::Search( const char* query, int flags, int filter ) const
{
    std::vector<std::string> terms;
    split( query, std::back_inserter( terms ) );
    return Search( terms, flags, filter );
}

static float HitRank( const PostData& data )
{
    auto ptr = data.hits;
    float ramp = 1.f + 2.0f * float( data.hitnum ) / std::numeric_limits<uint8_t>::max();
    float rank = LexiconHitRank( *ptr ) * ramp;
    for( int i=1; i<data.hitnum; i++ ) assert( LexiconHitRank( *ptr++ ) <= rank );
    return rank;
}

static float PostRank( const PostData& data )
{
    return ( float( data.children ) / LexiconChildMax ) * 0.75f + 0.25f;
}

static float GetWordDistance( const std::vector<const PostData*>& list )
{
    static thread_local std::vector<int> start[NUM_LEXICON_TYPES];
    static thread_local std::vector<std::vector<uint8_t>> hop[NUM_LEXICON_TYPES];

    const auto listsize = list.size();
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
        uint8_t data[256];
        int cnt = 0;
        const auto& src = list[i];
        for( int j=0; j<src->hitnum; j++ )
        {
            auto pos = LexiconHitPos( src->hits[j] );
            if( pos < LexiconHitPosMask[LexiconDecodeType( src->hits[j] )] )
            {
                data[cnt++] = src->hits[j];
            }
        }
        std::sort( data, data+cnt, []( const uint8_t l, const uint8_t r ) { return LexiconHitPos( l ) < LexiconHitPos( r ); } );

        for( int j=0; j<cnt; j++ )
        {
            auto& hit = data[j];

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
                            if( ad < 2 )
                            {
                                return 1;
                            }
                            else if( ad < min )
                            {
                                min = ad;
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

static SearchResult PrepareResults( uint32_t postid, float rank, int hitsize )
{
    SearchResult ret;
    const auto num = std::min<int>( hitsize, SearchResultMaxHits );
    ret.postid = postid;
    ret.rank = rank;
    ret.hitnum = num;
    for( int i=0; i<num-1; i++ ) assert( LexiconHitRank( i ) >= LexiconHitRank( i+1 ) );
    return ret;
}

uint32_t SearchEngine::ExtractWords( const std::vector<std::string>& terms, int flags, std::vector<WordData>& words, std::vector<const char*>& matched ) const
{
    std::unordered_set<uint32_t> wordset;
    uint32_t group = 0;
    words.reserve( terms.size() );
    for( auto& v : terms )
    {
        uint32_t wf = WF_None;
        const char* str = v.c_str();
        const char* strend = str + v.size();
        bool strictMatch = false;
        bool matchAll = false;
        if( flags & SF_SetLogic )
        {
            if( strend - str > 1 )
            {
                if( *str == '+' )
                {
                    wf |= WF_Must;
                    strictMatch = true;
                    str++;
                }
                else if( *str == '-' )
                {
                    wf |= WF_Cant;
                    strictMatch = true;
                    str++;
                }
            }
            if( strend - str > 5 )
            {
                if( strncmp( str, "from:", 5 ) == 0 )
                {
                    wf |= WF_From;
                    str += 5;
                }
                else if( strend - str > 8 )
                {
                    if( strncmp( str, "subject:", 8 ) == 0 )
                    {
                        wf |= WF_Subject;
                        str += 8;
                    }
                }
            }
            if( strend - str > 2 && *str == '"' && *(strend-1) == '"' )
            {
                str++;
                strend--;
                strictMatch = true;
            }
            if( !( wf & ( WF_Must | WF_Cant ) ) )
            {
                if( *(strend-1) == '*' )
                {
                    matchAll = true;
                    strend--;
                }
            }
        }

        std::vector<std::string> processed;
        if( !matchAll )
        {
            processed.emplace_back( str, strend );
        }
        else
        {
            auto& meta = m_archive.m_lexmeta;
            auto& data = m_archive.m_lexstr;
            const auto dataSize = meta.DataSize();
            for( uint32_t i=0; i<dataSize; i++ )
            {
                auto mp = meta + i;
                auto s = data + mp->str;
                if( strncmp( s, str, strend - str ) == 0 )
                {
                    processed.emplace_back( s );
                }
            }
        }

        for( auto& word : processed )
        {
            auto res = m_archive.m_lexhash.Search( word.c_str() );
            if( res >= 0 && wordset.find( res ) == wordset.end() )
            {
                words.emplace_back( WordData { uint32_t( res ), 1.f, wf, group++, strictMatch } );
                wordset.emplace( res );
                matched.emplace_back( m_archive.m_lexstr + m_archive.m_lexmeta[res].str );
            }
        }
    }

    if( flags & SF_FuzzySearch )
    {
        const auto sz = words.size();
        for( int i=0; i<sz; i++ )
        {
            const auto& wd = words[i];
            const auto flags = wd.flags;
            const auto group = wd.group;

            if( !wd.strict && !( flags & ( WF_Must | WF_Cant ) ) )
            {
                auto ptr = (*m_archive.m_lexdist)[wd.word];
                const auto size = *ptr++;
                for( uint32_t i=0; i<size; i++ )
                {
                    const auto data = *ptr++;
                    const auto offset = data & 0x3FFFFFFF;
                    auto word = m_archive.m_lexstr + offset;
                    auto res2 = m_archive.m_lexhash.Search( word );
                    assert( res2 >= 0 );
                    // todo: check if distance modifier is higher than already stored one
                    if( wordset.find( res2 ) == wordset.end() )
                    {
                        wordset.emplace( res2 );
                        const auto dist = data >> 30;
                        assert( dist > 0 && dist <= 3 );
                        static const float DistMod[] = { 0.f, 0.01f, 0.001f, 0.0001f };
                        words.emplace_back( WordData { uint32_t( res2 ), DistMod[dist], flags, group, false } );
                        matched.emplace_back( word );
                    }
                }
            }
        }
    }

    return group;
}

std::vector<SearchEngine::PostDataVec> SearchEngine::GetPostsForWords( const std::vector<WordData>& words, int filter ) const
{
    std::vector<PostDataVec> wdata;
    wdata.reserve( words.size() );

    for( int w=0; w<words.size(); w++ )
    {
        const auto v = words[w].word;
        const auto wf = words[w].flags;

        auto meta = m_archive.m_lexmeta[v];
        auto data = m_archive.m_lexdata + ( meta.data / sizeof( LexiconDataPacket ) );

        const auto allocSize = meta.dataSize;
        if( allocSize * sizeof( PostData ) > SlabSize )
        {
            wdata.emplace_back( 0, nullptr );
            continue;
        }
        auto pdata = (PostData*)slab.Alloc( sizeof( PostData ) * allocSize );
        auto ptr = pdata;

        for( uint32_t i=0; i<meta.dataSize; i++ )
        {
            uint8_t children = data->postid >> LexiconChildShift;
            uint8_t hitnum = data->hitoffset >> LexiconHitShift;
            const uint8_t* hits;
            if( hitnum == 0 )
            {
                hits = m_archive.m_lexhit + ( data->hitoffset & LexiconHitOffsetMask );
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
                        *ptr++ = PostData { data->postid & LexiconPostMask, hitnum, children, hits };
                        break;
                    }
                }
            }
            else if( wf & ( WF_From | WF_Subject ) )
            {
                const LexiconType type = ( wf & WF_From ) ? T_From : T_Subject;
                for( int j=0; j<hitnum; j++ )
                {
                    if( LexiconDecodeType( hits[j] ) == type )
                    {
                        *ptr++ = PostData { data->postid & LexiconPostMask, hitnum, children, hits };
                        break;
                    }
                }
            }
            else
            {
                *ptr++ = PostData { data->postid & LexiconPostMask, hitnum, children, hits };
            }
            data++;
        }

        const auto psize = ptr - pdata;
        assert( psize <= allocSize );
        if( psize != allocSize )
        {
            slab.Unalloc( sizeof( PostData ) * ( allocSize - psize ) );
        }
        wdata.emplace_back( psize, pdata );
    }

    return wdata;
}

int SearchEngine::FixupFlags( int flags ) const
{
    if( flags & SF_FuzzySearch )
    {
        if( m_archive.m_lexdist )
        {
            flags &= ~SF_RequireAllWords;
        }
        else
        {
            flags &= ~SF_FuzzySearch;
        }
    }
    if( flags & SF_RequireAllWords )
    {
        flags &= ~SF_SetLogic;
    }
    return flags;
}

std::vector<SearchResult> SearchEngine::GetSingleResult( const std::vector<SearchEngine::PostDataVec>& wdata ) const
{
    std::vector<SearchResult> result;

    assert( wdata.size() == 1 );

    const auto size = wdata[0].first;
    auto ptr = wdata[0].second;
    auto end = ptr + size;
    result.reserve( size );
    while( ptr != end )
    {
        // hits are already sorted
        result.emplace_back( PrepareResults( ptr->postid, PostRank( *ptr ) * HitRank( *ptr ), ptr->hitnum ) );
        auto& sr = result.back();
        memcpy( sr.hits, ptr->hits, sr.hitnum );
        memset( sr.words, 0, sr.hitnum * sizeof( uint32_t ) );
        ptr++;
    }

    return result;
}

std::vector<SearchResult> SearchEngine::GetAllWordResult( const std::vector<SearchEngine::PostDataVec>& wdata, int flags ) const
{
    std::vector<SearchResult> result;
    const auto wsize = wdata.size();

    std::vector<const PostData*> list;
    list.reserve( 32 );
    result.reserve( wdata[0].first );

    auto& vec = *wdata.begin();
    for( int i=0; i<vec.first; i++ )
    {
        auto& post = vec.second[i];
        list.clear();
        bool ok = true;
        for( size_t i=1; i<wsize; i++ )
        {
            auto& vtest = wdata[i];
            auto begin = vtest.second;
            auto end = begin + vtest.first;
            auto it = std::lower_bound( begin, end, post, [] ( const auto& l, const auto& r ) { return l.postid < r.postid; } );
            if( it == end || it->postid != post.postid )
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
                if( list.size() != 1 )
                {
                    rank /= GetWordDistance( list );
                }
                else
                {
                    rank /= 127;
                }
            }
            // only used in threadify, no need to output hit data
            result.emplace_back( PrepareResults( post.postid, rank * PostRank( post ), 0 ) );
        }
    }

    return result;
}

std::vector<SearchResult> SearchEngine::GetFullResult( const std::vector<SearchEngine::PostDataVec>& wdata, const std::vector<WordData>& words, int flags ) const
{
    std::vector<SearchResult> result;
    const auto wsize = std::min<size_t>( 1024, wdata.size() );

    bool checkInclude = false;
    std::unordered_set<uint32_t> include;
    if( flags & SF_SetLogic )
    {
        bool hasMust = false;
        bool hasCant = false;

        for( auto& word : words )
        {
            auto flag = word.flags;
            if( flag & ( WF_Must | WF_Cant ) )
            {
                checkInclude = true;

                if( flag & WF_Must )
                {
                    hasMust = true;
                }
                else
                {
                    hasCant = true;
                }
            }
        }
        if( checkInclude )
        {
            if( hasMust )
            {
                int i;
                for( i=0; i<wdata.size(); i++ )
                {
                    if( words[i].flags & WF_Must )
                    {
                        for( int j=0; j<wdata[i].first; j++ )
                        {
                            auto& v = wdata[i].second[j];
                            include.emplace( v.postid );
                        }
                        break;
                    }
                }
                for( i=i+1; i<wdata.size(); i++ )
                {
                    if( words[i].flags & WF_Must )
                    {
                        auto& vtest = wdata[i];
                        auto begin = vtest.second;
                        auto end = begin + vtest.first;
                        auto it = include.begin();
                        while( it != include.end() )
                        {
                            auto vit = std::lower_bound( begin, end, *it, [] ( const auto& l, const auto& r ) { return l.postid < r; } );
                            if( vit == end || vit->postid != *it )
                            {
                                it = include.erase( it );
                            }
                            else
                            {
                                ++it;
                            }
                        }
                    }
                }
            }
            else
            {
                for( int i=0; i<wdata.size(); i++ )
                {
                    if( !( words[i].flags & WF_Cant ) )
                    {
                        assert( !( words[i].flags & WF_Must ) );
                        for( int j=0; j<wdata[i].first; j++ )
                        {
                            auto& v = wdata[i].second[j];
                            include.emplace( v.postid );
                        }
                    }
                }
            }
            if( include.empty() ) return result;

            if( hasCant )
            {
                for( int i=0; i<wdata.size(); i++ )
                {
                    if( words[i].flags & WF_Cant )
                    {
                        for( int j=0; j<wdata[i].first; j++ )
                        {
                            auto& v = wdata[i].second[j];
                            auto it = include.find( v.postid );
                            if( it != include.end() )
                            {
                                include.erase( it );
                            }
                        }
                    }
                }
                if( include.empty() ) return result;
            }
        }
    }

    struct Posts
    {
        uint32_t word;
        const PostData* data;
    };

    int count = 0;
    for( uint32_t word = 0; word < wsize; word++ )
    {
        count += wdata[word].first;
    }

    auto index = new int32_t[m_archive.NumberOfMessages()];
    memset( index, 0xFF, sizeof( int32_t ) * m_archive.NumberOfMessages() );

    auto pnum = new uint32_t[count];
    auto postid = new uint32_t[count];
    auto pdata = new Posts[count*wsize];

    int next = 0;
    for( uint32_t word = 0; word < wsize; word++ )
    {
        for( int i=0; i<wdata[word].first; i++ )
        {
            auto& post = wdata[word].second[i];
            auto pidx = post.postid;
            if( !checkInclude || include.find( pidx ) != include.end() )
            {
                int idx;
                if( index[pidx] == -1 )
                {
                    index[pidx] = next;
                    idx = next++;
                    pnum[idx] = 0;
                    postid[idx] = pidx;
                }
                else
                {
                    idx = index[pidx];
                }
                pdata[idx*wsize + pnum[idx]] = Posts { word, &post };
                pnum[idx]++;
            }
        }
    }

    delete[] index;

    std::vector<const PostData*> list;
    std::vector<uint8_t> hits;
    std::vector<uint32_t> wordlist;
    std::vector<uint32_t> idx;
    result.reserve( next );
    for( int k=0; k<next; k++ )
    {
        hits.clear();
        wordlist.clear();
        idx.clear();

        float rank = 0;
        for( int m=0; m<pnum[k]; m++ )
        {
            auto& v = pdata[k*wsize + m];
            rank += HitRank( *v.data ) * words[v.word].mod;
            for( int i=0; i<v.data->hitnum; i++ )
            {
                wordlist.emplace_back( v.word );
                hits.emplace_back( v.data->hits[i] );
            }
        }
        if( flags & SF_AdjacentWords )
        {
            if( pnum[k] != 1 )
            {
                list.clear();
                list.reserve( pnum[k] );
                for( int m=0; m<pnum[k]; m++ )
                {
                    list.emplace_back( pdata[k*wsize + m].data );
                }
                rank /= GetWordDistance( list );
            }
            else
            {
                rank /= 127;
            }
        }
        idx.reserve( hits.size() );
        for( int i=0; i<hits.size(); i++ )
        {
            idx.emplace_back( i );
        }

        int idxsize = idx.size();
        if( idxsize > 1 )
        {
            std::sort( idx.begin(), idx.end(), [&hits]( const auto& l, const auto& r ) { return LexiconHitRank( hits[l] ) > LexiconHitRank( hits[r] ); } );

            bool hitmask[256];
            memset( &hitmask, 0, sizeof( hitmask ) );
            int i=0;
            while( i<idxsize )
            {
                const auto hit = hits[idx[i]];
                if( !LexiconHitIsMaxPos( hit ) )
                {
                    if( !hitmask[hit] )
                    {
                        hitmask[hit] = true;
                        i++;
                    }
                    else
                    {
                        idx.erase( idx.begin() + i );
                        idxsize--;
                    }
                }
                else
                {
                    i++;
                }
            }
        }

        result.emplace_back( PrepareResults( postid[k], rank * PostRank( *pdata[k*wsize].data ), idxsize ) );
        auto& sr = result.back();
        const auto n = sr.hitnum;
        for( int i=0; i<n; i++ )
        {
            sr.hits[i] = hits[idx[i]];
            sr.words[i] = wordlist[idx[i]];
        }
    }

    delete[] pnum;
    delete[] postid;
    delete[] pdata;

    return result;
}

SearchData SearchEngine::Search( const std::vector<std::string>& terms, int flags, int filter ) const
{
    SearchData ret;

    flags = FixupFlags( flags );

    std::vector<WordData> words;
    std::vector<const char*> matched;

    if( ExtractWords( terms, flags, words, matched ) == 0 ) return ret;
    if( words.size() == 1 && words[0].flags & WF_Cant ) return ret;

    const auto wdata = GetPostsForWords( words, filter );

    std::vector<SearchResult> result;

    if( wdata.size() == 1 )
    {
        result = GetSingleResult( wdata );
    }
    else if( flags & SF_RequireAllWords )
    {
        assert( !( flags & SF_SetLogic ) );
        result = GetAllWordResult( wdata, flags );
    }
    else
    {
        result = GetFullResult( wdata, words, flags );
    }

    slab.Reset();

    if( result.empty() ) return ret;

    std::sort( result.begin(), result.end(), []( const auto& l, const auto& r ) { return l.rank > r.rank; } );

    std::swap( ret.matched, matched );
    std::swap( ret.results, result );

    return ret;
}
