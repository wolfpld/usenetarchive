#include "Archive.hpp"

Archive::Archive( const std::string& dir )
    : m_mview( dir + "/zmeta", dir + "/zdata", dir + "/zdict" )
    , m_mcnt( m_mview.Size() )
{
}

const char* Archive::GetMessage( uint32_t idx )
{
    return idx >= m_mcnt ? nullptr : m_mview[idx];
}
