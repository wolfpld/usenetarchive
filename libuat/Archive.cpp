#include "Archive.hpp"

Archive::Archive( const std::string& dir )
    : m_mview( dir + "/zmeta", dir + "/zdata", dir + "/zdict" )
    , m_mcnt( m_mview.Size() )
    , m_toplevel( dir + "/toplevel" )
{
}

const char* Archive::GetMessage( uint32_t idx )
{
    return idx >= m_mcnt ? nullptr : m_mview[idx];
}

ViewReference<uint32_t> Archive::GetTopLevel() const
{
    return ViewReference<uint32_t> { m_toplevel, m_toplevel.DataSize() };
}
