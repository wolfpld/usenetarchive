#ifndef __SEARCHENGINE_HPP__
#define __SEARCHENGINE_HPP__

#include <stdint.h>
#include <vector>

#include "../common/LexiconTypes.hpp"

class Archive;

enum { SearchResultMaxHits = 4 };

struct SearchResult
{
    uint32_t postid;
    float rank;
    uint8_t hitnum;
    uint8_t hits[SearchResultMaxHits];
    uint32_t words[SearchResultMaxHits];
};

struct SearchData
{
    std::vector<SearchResult> results;
    std::vector<const char*> matched;
};

struct WordData
{
    uint32_t word;
    float mod;
    uint32_t flags : 4;     // WordFlags
    uint32_t group : 27;
    uint32_t strict : 1;
};
static_assert( sizeof( WordData ) == 12, "Wrong word data struct size" );

struct PostData;

class SearchEngine
{
public:
    enum SearchFlags
    {
        SF_FlagsNone        = 0,
        SF_AdjacentWords    = 1 << 0,   // Calculate words adjacency
        SF_RequireAllWords  = 1 << 1,   // Require all words to be present
        SF_FuzzySearch      = 1 << 2,   // Also search for similar words
        SF_SetLogic         = 1 << 3,   // Parse set logic functions (search in headers, search for exact words, etc.)
    };

    SearchEngine( const Archive& archive );

    SearchData Search( const char* query, int flags = SF_FlagsNone, int filter = T_All ) const;
    SearchData Search( const std::vector<std::string>& terms, int flags = SF_FlagsNone, int filter = T_All ) const;

private:
    using PostDataVec = std::pair<uint32_t, PostData*>;

    uint32_t ExtractWords( const std::vector<std::string>& terms, int flags, std::vector<WordData>& words, std::vector<const char*>& matched ) const;
    std::vector<PostDataVec> GetPostsForWords( const std::vector<WordData>& words, int filter ) const;
    int FixupFlags( int flags ) const;

    std::vector<SearchResult> GetSingleResult( const std::vector<PostDataVec>& wdata ) const;
    std::vector<SearchResult> GetAllWordResult( const std::vector<PostDataVec>& wdata, int flags, uint32_t groups, uint32_t missing ) const;
    std::vector<SearchResult> GetFullResult( const std::vector<PostDataVec>& wdata, const std::vector<WordData>& words, int flags, uint32_t groups, uint32_t missing ) const;

    const Archive& m_archive;
};

#endif
