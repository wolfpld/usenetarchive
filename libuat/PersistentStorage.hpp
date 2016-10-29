#ifndef __PERSISTENTSTORAGE_HPP__
#define __PERSISTENTSTORAGE_HPP__

#include <stdint.h>
#include <string>
#include <string.h>
#include <unordered_set>
#include <vector>

#include "../contrib/xxhash/xxhash.h"

class PersistentStorage
{
public:
    PersistentStorage();
    ~PersistentStorage();

    void WriteLastOpenArchive( const char* archive );
    std::string ReadLastOpenArchive();

    void WriteLastArticle( const char* archive, uint32_t idx );
    uint32_t ReadLastArticle( const char* archive );

    bool WasVisited( const char* msgid );
    bool MarkVisited( const char* msgid );

private:
    struct hash { size_t operator()( const char* v ) const { return XXH32( v, strlen( v ), 0 ); } };
    struct equal_to { bool operator()( const char* l, const char* r ) const { return strcmp( l, r ) == 0; } };

    std::string CreateLastArticleFilename( const char* archive );
    const char* StoreString( const char* str );
    void VerifyVisitedAreValid( const std::string& fn );

    std::string m_base;
    std::unordered_set<const char*, hash, equal_to> m_visited;
    uint64_t m_visitedTimestamp;

    std::vector<char*> m_buffers;
    char* m_currBuf;
    size_t m_bufLeft;
};

#endif
