#include <assert.h>
#include <unordered_set>

#include "Archive.hpp"
#include "SearchEngine.hpp"

#include "../common/String.hpp"

enum WordFlags
{
    WF_None     = 0,
    WF_Must     = 1 << 0,
    WF_Cant     = 1 << 1,
    WF_Header   = 1 << 2
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
    float rank = LexiconHitRank( *ptr );
    for( int i=1; i<data.hitnum; i++ ) assert( LexiconHitRank( *ptr++ ) <= rank );
    return rank;
}

static float PostRank( const PostData& data )
{
    return ( float( data.children ) / LexiconChildMax ) * 0.9f + 0.1f;
}

static float GetWordDistance( const std::vector<const PostData*>& list )
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

static SearchResult PrepareResults( uint32_t postid, float rank, int hitsize, const uint8_t* hitdata, const uint32_t* words )
{
    SearchResult ret;
    const auto num = std::min<int>( hitsize, SearchResultMaxHits );
    ret.postid = postid;
    ret.rank = rank;
    ret.hitnum = num;
    memcpy( ret.hits, hitdata, num );
    for( int i=0; i<num-1; i++ ) assert( LexiconHitRank( i ) >= LexiconHitRank( i+1 ) );
    memcpy( ret.words, words, num * sizeof( uint32_t ) );
    return ret;
}

bool SearchEngine::ExtractWords( const std::vector<std::string>& terms, int flags, std::vector<uint32_t>& words, std::vector<int>& wordFlags, std::vector<float>& wordMod, std::vector<const char*>& matched ) const
{
    std::unordered_set<uint32_t> wordset;
    words.reserve( terms.size() );
    wordFlags.reserve( terms.size() );
    for( auto& v : terms )
    {
        int wf = WF_None;
        const char* str = v.c_str();
        const char* strend = str + v.size();
        bool strictMatch = false;
        std::string tmp;
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
            if( strend - str > 4 )
            {
                if( strncmp( str, "hdr:", 4 ) == 0 )
                {
                    wf |= WF_Header;
                    str += 4;
                }
            }
            if( strend - str > 2 && *str == '"' && *(strend-1) == '"' )
            {
                str++;
                strend--;
                strictMatch = true;
            }
            if( strend - str != v.size() )
            {
                tmp = std::string( str, strend );
                str = tmp.c_str();
            }
        }

        auto res = m_archive.m_lexhash.Search( str );
        if( res >= 0 && wordset.find( res ) == wordset.end() )
        {
            words.emplace_back( res );
            wordFlags.emplace_back( wf );
            wordset.emplace( res );
            wordMod.emplace_back( 1 );
            matched.emplace_back( m_archive.m_lexstr + m_archive.m_lexmeta[res].str );

            if( flags & SF_FuzzySearch && !strictMatch && !( wf & ( WF_Must | WF_Cant ) ) )
            {
                auto ptr = (*m_archive.m_lexdist)[res];
                const auto size = *ptr++;
                for( uint32_t i=0; i<size; i++ )
                {
                    const auto data = *ptr++;
                    const auto offset = data & 0x3FFFFFFF;
                    auto word = m_archive.m_lexstr + offset;
                    auto res2 = m_archive.m_lexhash.Search( word );
                    assert( res2 >= 0 );
                    if( wordset.find( res2 ) == wordset.end() )
                    {
                        assert( !( wf & ( WF_Must | WF_Cant ) ) );
                        words.emplace_back( res2 );
                        wordFlags.emplace_back( wf );
                        wordset.emplace( res2 );
                        const auto dist = data >> 30;
                        assert( dist >= 0 && dist <= 3 );
                        static const float DistMod[] = { 0.125f, 0.5f, 0.25f, 0.125f };
                        wordMod.emplace_back( DistMod[dist] );
                        matched.emplace_back( word );
                    }
                }
            }
        }
    }

    return !words.empty();
}

std::vector<std::vector<PostData>> SearchEngine::GetPostsForWords( const std::vector<uint32_t>& words, const std::vector<int>& wordFlags, int filter ) const
{
    std::vector<std::vector<PostData>> wdata;
    wdata.reserve( words.size() );

    for( int w=0; w<words.size(); w++ )
    {
        const auto v = words[w];
        const auto wf = wordFlags[w];

        auto meta = m_archive.m_lexmeta[v];
        auto data = m_archive.m_lexdata + ( meta.data / sizeof( LexiconDataPacket ) );

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
                        vec.emplace_back( PostData { data->postid & LexiconPostMask, hitnum, children, hits } );
                        break;
                    }
                }
            }
            else if( wf & WF_Header )
            {
                for( int j=0; j<hitnum; j++ )
                {
                    if( LexiconDecodeType( hits[j] ) == T_Header )
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

std::vector<SearchResult> SearchEngine::GetSingleResult( const std::vector<std::vector<PostData>>& wdata ) const
{
    std::vector<SearchResult> result;

    assert( wdata.size() == 1 );

    uint32_t wordlist[SearchResultMaxHits] = {};
    result.reserve( wdata[0].size() );
    for( auto& v : wdata[0] )
    {
        // hits are already sorted
        result.emplace_back( PrepareResults( v.postid, PostRank( v ) * HitRank( v ), v.hitnum, v.hits, wordlist ) );
    }

    return result;
}

std::vector<SearchResult> SearchEngine::GetAllWordResult( const std::vector<std::vector<PostData>>& wdata, int flags ) const
{
    std::vector<SearchResult> result;
    const auto wsize = wdata.size();

    std::vector<const PostData*> list;
    list.reserve( 32 );
    result.reserve( wdata[0].size() );

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
            result.emplace_back( PrepareResults( post.postid, rank * PostRank( post ), 0, nullptr, nullptr ) );
        }
    }

    return result;
}

std::vector<SearchResult> SearchEngine::GetFullResult( const std::vector<std::vector<PostData>>& wdata, const std::vector<float>& wordMod, int flags ) const
{
    std::vector<SearchResult> result;
    const auto wsize = wdata.size();

    struct Posts
    {
        uint32_t word;
        const PostData* data;
    };

    int count = 0;
    for( uint32_t word = 0; word < wsize; word++ )
    {
        count += wdata[word].size();
    }

    auto index = new int32_t[m_archive.NumberOfMessages()];
    memset( index, 0xFF, sizeof( int32_t ) * m_archive.NumberOfMessages() );

    auto pnum = new uint32_t[count];
    auto postid = new uint32_t[count];
    auto pdata = new Posts[count*wsize];

    int next = 0;
    for( uint32_t word = 0; word < wsize; word++ )
    {
        for( auto& post : wdata[word] )
        {
            auto pidx = post.postid;
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
            rank += HitRank( *v.data ) * wordMod[v.word];
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

        if( idx.size() > 1 )
        {
            std::sort( idx.begin(), idx.end(), [&hits]( const auto& l, const auto& r ) { return LexiconHitRank( hits[l] ) > LexiconHitRank( hits[r] ); } );
        }

        uint8_t hdata[SearchResultMaxHits];
        uint32_t wdata[SearchResultMaxHits];
        const auto n = std::min<int>( hits.size(), SearchResultMaxHits );
        for( int i=0; i<n; i++ )
        {
            hdata[i] = hits[idx[i]];
            wdata[i] = wordlist[idx[i]];
        }

        result.emplace_back( PrepareResults( postid[k], rank * PostRank( *pdata[k*wsize].data ), n, hdata, wdata ) );
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

    std::vector<uint32_t> words;
    std::vector<int> wordFlags;
    std::vector<float> wordMod;
    std::vector<const char*> matched;

    if( !ExtractWords( terms, flags, words, wordFlags, wordMod, matched ) ) return ret;
    if( wordFlags.size() == 1 && wordFlags[0] & WF_Cant ) return ret;

    const auto wdata = GetPostsForWords( words, wordFlags, filter );

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
        result = GetFullResult( wdata, wordMod, flags );
    }

    if( result.empty() ) return ret;

    std::sort( result.begin(), result.end(), []( const auto& l, const auto& r ) { return l.rank > r.rank; } );

    std::swap( ret.matched, matched );
    std::swap( ret.results, result );

    return ret;
}
