#ifndef __SEARCHENGINE_HPP__
#define __SEARCHENGINE_HPP__

#include <stdint.h>
#include <vector>

#include "../common/LexiconTypes.hpp"

class Archive;

enum { SearchResultMaxHits = 7 };

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

class SearchEngine
{
public:
    enum SearchFlags
    {
        SF_FlagsNone        = 0,
        SF_AdjacentWords    = 1 << 0,   // Calculate words adjacency
        SF_RequireAllWords  = 1 << 1,   // Require all words to be present
        SF_FuzzySearch      = 1 << 2,   // Also search for similar words
    };

    SearchEngine( const Archive& archive );

    SearchData Search( const char* query, int flags = SF_FlagsNone, int filter = T_All ) const;
    SearchData Search( const std::vector<std::string>& terms, int flags = SF_FlagsNone, int filter = T_All ) const;

private:
    const Archive& m_archive;
};

#endif
