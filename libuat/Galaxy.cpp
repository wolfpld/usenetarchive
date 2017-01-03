#include "../common/Filesystem.hpp"

#include "Galaxy.hpp"

Galaxy* Galaxy::Open( const std::string& fn )
{
    if( !Exists( fn ) || IsFile( fn ) ) return nullptr;

    auto base = fn;
    if( fn.back() != '/' ) base += '/';
    if( !Exists( base + "archives" ) || !Exists( base + "archives.meta" ) ||
        !Exists( base + "midgr" ) || !Exists( base + "midgr.meta" ) ||
        !Exists( base + "midhash" ) || !Exists( base + "midhash.meta" ) ||
        !Exists( base + "msgid" ) || !Exists( base + "msgid.meta" ) ||
        !Exists( base + "str" ) || !Exists( base + "str.meta" ) )
    {
        return nullptr;
    }
    else
    {
        return new Galaxy( base );
    }
}

Galaxy::Galaxy( const std::string& fn )
    : m_base( fn )
    , m_middb( fn + "msgid.meta", fn + "msgid" )
    , m_midhash( fn + "msgid.meta", fn + "msgid", fn + "midhash.meta", fn + "midhash" )
    , m_archives( fn + "archives.meta", fn + "archives" )
    , m_strings( fn + "str.meta", fn + "str" )
    , m_midgr( fn + "midgr.meta", fn + "midgr" )
{
    const auto size = m_archives.Size() / 2;
    for( int i=0; i<size; i++ )
    {
        const auto path = std::string( m_archives[i*2], m_archives[i*2+1] );
        if( Exists( path ) )
        {
            m_arch.emplace_back( Archive::Open( path ) );
            m_available.emplace_back( i );
        }
        else
        {
            const auto relative = m_base + path;
            if( Exists( relative ) )
            {
                m_arch.emplace_back( Archive::Open( relative ) );
                m_available.emplace_back( i );
            }
            else
            {
                m_arch.emplace_back( nullptr );
            }
        }
    }
}

std::string Galaxy::GetArchiveName( int idx ) const
{
    return std::string( m_archives[idx*2], m_archives[idx*2+1] );
}
