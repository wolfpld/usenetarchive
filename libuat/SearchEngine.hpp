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
    bool ExtractWords( const std::vector<std::string>& terms, int flags, std::vector<uint32_t>& words, std::vector<int>& wordFlags, std::vector<float>& wordMod, std::vector<const char*>& matched ) const;
    std::vector<std::vector<PostData>> GetPostsForWords( const std::vector<uint32_t>& words, const std::vector<int>& wordFlags, int filter ) const;
    int FixupFlags( int flags ) const;

    std::vector<SearchResult> GetSingleResult( const std::vector<std::vector<PostData>>& wdata ) const;
    std::vector<SearchResult> GetAllWordResult( const std::vector<std::vector<PostData>>& wdata, int flags ) const;
    std::vector<SearchResult> GetFullResult( const std::vector<std::vector<PostData>>& wdata, const std::vector<float>& wordMod, const std::vector<int>& wordFlags, int flags ) const;

    const Archive& m_archive;
};

#endif
