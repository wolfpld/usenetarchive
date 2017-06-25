#include <assert.h>
#include <unordered_map>
#include <unordered_set>

#include "Archive.hpp"
#include "SearchEngine.hpp"

#include "../common/String.hpp"

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

SearchData SearchEngine::Search( const std::vector<std::string>& terms, int flags, int filter ) const
{
    SearchData ret;

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

    std::vector<const char*> matched;
    std::vector<uint32_t> words;
    std::vector<float> wordMod;

    {
        std::unordered_set<uint32_t> wordset;
        words.reserve( terms.size() );
        for( auto& v : terms )
        {
            const char* str = v.c_str();
            std::string tmp;
            if( v.size() > 2 && v[0] == '"' && v[v.size()-1] == '"' )
            {
                tmp = v.substr( 1, v.size() - 2 );
                str = tmp.c_str();
            }

            auto res = m_archive.m_lexhash.Search( str );
            if( res >= 0 && wordset.find( res ) == wordset.end() )
            {
                words.emplace_back( res );
                wordset.emplace( res );
                wordMod.emplace_back( 1 );
                matched.emplace_back( m_archive.m_lexstr + m_archive.m_lexmeta[res].str );

                if( flags & SF_FuzzySearch && tmp.empty() )
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
                            words.emplace_back( res2 );
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
    }
    if( words.empty() ) return ret;

    std::vector<std::vector<PostData>> wdata;
    wdata.reserve( words.size() );
    for( auto& v : words )
    {
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
