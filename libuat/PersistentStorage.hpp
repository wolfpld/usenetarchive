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

private:
    std::string m_base;
};

#endif
