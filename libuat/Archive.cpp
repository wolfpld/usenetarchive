#include "Archive.hpp"

Archive::Archive( const std::string& dir )
    : m_mview( dir + "/zmeta", dir + "/zdata", dir + "/zdict" )
    , m_mcnt( m_mview.Size() )
    , m_toplevel( dir + "/toplevel" )
    , m_midhash( dir + "/middata", dir + "/midhash", dir + "/midhashdata" )
    , m_connectivity( dir + "/connmeta", dir + "/conndata" )
    , m_strings( dir + "/strmeta", dir + "/strings" )
    , m_lexmeta( dir + "/lexmeta" )
    , m_lexstr( dir + "/lexstr" )
    , m_lexdata( dir + "/lexdata" )
    , m_lexhit( dir + "/lexhit" )
    , m_lexhash( dir + "/lexstr", dir + "/lexhash", dir + "/lexhashdata" )
{
}

const char* Archive::GetMessage( uint32_t idx )
{
    return idx >= m_mcnt ? nullptr : m_mview[idx];
}

const char* Archive::GetMessage( const char* msgid )
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetMessage( idx ) : nullptr;
}

ViewReference<uint32_t> Archive::GetTopLevel() const
{
    return ViewReference<uint32_t> { m_toplevel, m_toplevel.DataSize() };
}

int32_t Archive::GetParent( uint32_t idx ) const
{
    auto data = m_connectivity[idx];
    data++;
    return (int32_t)*data;
}

int32_t Archive::GetParent( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetParent( idx ) : -1;
}

ViewReference<uint32_t> Archive::GetChildren( uint32_t idx ) const
{
    auto data = m_connectivity[idx];
    data += 2;
    auto num = *data++;
    return ViewReference<uint32_t> { data, num };
}

ViewReference<uint32_t> Archive::GetChildren( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetChildren( idx ) : ViewReference<uint32_t> { nullptr, 0 };
}

uint32_t Archive::GetDate( uint32_t idx ) const
{
    auto data = m_connectivity[idx];
    return *data;
}

uint32_t Archive::GetDate( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetDate( idx ) : 0;
}

const char* Archive::GetFrom( uint32_t idx ) const
{
    return m_strings[idx*2];
}

const char* Archive::GetFrom( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetFrom( idx ) : nullptr;
}

const char* Archive::GetSubject( uint32_t idx ) const
{
    return m_strings[idx*2+1];
}

const char* Archive::GetSubject( const char* msgid ) const
{
    auto idx = m_midhash.Search( msgid );
    return idx >= 0 ? GetSubject( idx ) : nullptr;
}
