#include "LockedFile.hpp"

LockedFile::LockedFile( const char* fn )
    : m_fn( fn )
    , m_lock( fn )
{
}

LockedFile::~LockedFile()
{
}
