#ifndef __PERSISTENTSTORAGE_HPP__
#define __PERSISTENTSTORAGE_HPP__

#include <string>

class PersistentStorage
{
public:
    PersistentStorage();
    ~PersistentStorage();

    void WriteLastOpenArchive( const char* archive );
    std::string ReadLastOpenArchive();

    void WriteLastArticle( const char* archive, uint32_t idx );
    uint32_t ReadLastArticle( const char* archive );

private:
    std::string CreateLastArticleFilename( const char* archive );

    std::string m_base;
};

#endif
