#ifndef __LOCKEDFILE_HPP__
#define __LOCKEDFILE_HPP__

#include <string>

#include "named_mutex.hpp"

class LockedFile
{
public:
    LockedFile( const char* fn ) : m_fn( fn ), m_lock( fn ) {}

    void lock() { m_lock.lock(); }
    void unlock() { m_lock.unlock(); }

    operator const char*() const { return m_fn.c_str(); }
    operator const std::string&() const { return m_fn; }

private:
    const std::string m_fn;
    named_mutex m_lock;
};

#endif
