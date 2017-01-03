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
{
}
