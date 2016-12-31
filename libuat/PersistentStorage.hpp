#ifndef __PERSISTENTSTORAGE_HPP__
#define __PERSISTENTSTORAGE_HPP__

#include <chrono>
#include <stdint.h>
#include <string>
#include <string.h>
#include <unordered_set>
#include <vector>

#include "../contrib/xxhash/xxhash.h"
#include "../common/ring_buffer.hpp"

#include "LockedFile.hpp"

struct ScoreEntry;

class PersistentStorage
{
public:
    PersistentStorage();
    ~PersistentStorage();

    void WriteLastOpenArchive( const char* archive );
    std::string ReadLastOpenArchive();

    void WriteArticleHistory( const char* archive );
    bool ReadArticleHistory( const char* archive );

    void AddToHistory( uint32_t idx );
    const ring_buffer<uint32_t>& GetArticleHistory() const { return m_articleHistory; }

    bool WasVisited( const char* msgid );
    bool MarkVisited( const char* msgid );

private:
    struct hash { size_t operator()( const char* v ) const { return XXH32( v, strlen( v ), 0 ); } };
    struct equal_to { bool operator()( const char* l, const char* r ) const { return strcmp( l, r ) == 0; } };

    std::string CreateLastArticleFilename( const char* archive );
    const char* StoreString( const char* str );
    void VerifyVisitedAreValid( const std::string& fn );

    void LoadScore();

    std::string m_base;
    std::unordered_set<const char*, hash, equal_to> m_visited;
    uint64_t m_visitedTimestamp;
    LockedFile m_visitedGuard;
    std::chrono::steady_clock::time_point m_visitedLastVerify;

    std::vector<char*> m_buffers;
    char* m_currBuf;
    size_t m_bufLeft;

    ring_buffer<uint32_t> m_articleHistory;
    std::vector<ScoreEntry> m_scoreList;
};

#endif
