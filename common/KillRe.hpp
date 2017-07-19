#ifndef __KILLRE_HPP__
#define __KILLRE_HPP__

#include <vector>

class KillRe
{
public:
    KillRe();
    ~KillRe();

    const char* Kill( const char* str ) const;

    void Add( const char* str );
    void Add( const char* begin, const char* end );

private:
    KillRe( const KillRe& ) = delete;
    KillRe( KillRe&& ) = delete;

    KillRe& operator=( const KillRe& ) = delete;
    KillRe& operator=( KillRe&& ) = delete;

    std::vector<const char*> m_prefix;
};

#endif
