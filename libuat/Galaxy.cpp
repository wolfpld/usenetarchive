#include <algorithm>

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
        !Exists( base + "str" ) || !Exists( base + "str.meta" ) ||
        !Exists( base + "indirect" ) || !Exists( base + "indirect.offset" ) || !Exists( base + "indirect.dense" ) )
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
    , m_midhash( fn + "msgid", fn + "midhash.meta", fn + "midhash" )
    , m_archives( fn + "archives.meta", fn + "archives" )
    , m_strings( fn + "str.meta", fn + "str" )
    , m_midgr( fn + "midgr.meta", fn + "midgr" )
    , m_indirect( fn + "indirect.offset", fn + "indirect" )
    , m_indirectDense( fn + "indirect.dense" )
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

const std::shared_ptr<Archive>& Galaxy::GetArchive( int idx, bool change )
{
    if( change )
    {
        m_active = idx;
    }
    return m_arch[idx];
}

bool Galaxy::AreChildrenSame( uint32_t idx, const char* msgid ) const
{
    auto ptr = m_midgr[idx];
    auto num = *ptr++;

    std::vector<Archive*> arch;
    for( int i=0; i<num; i++ )
    {
        if( IsArchiveAvailable( *ptr ) )
        {
            arch.emplace_back( m_arch[*ptr].get() );
        }
        ptr++;
    }
    assert( !arch.empty() );
    if( arch.size() == 1 ) return true;

    auto children = arch[0]->GetChildren( msgid );
    for( int i=1; i<arch.size(); i++ )
    {
        if( arch[i]->GetChildren( msgid ).size != children.size )
        {
            return false;
        }
    }

    std::vector<const char*> test;
    for( int i=0; i<children.size; i++ )
    {
        test.emplace_back( arch[0]->GetMessageId( children.ptr[i] ) );
    }

    for( int i=1; i<arch.size(); i++ )
    {
        auto c2 = arch[i]->GetChildren( msgid );
        for( int j=0; j<c2.size; j++ )
        {
            auto tmid = arch[i]->GetMessageId( c2.ptr[j] );
            bool found = false;
            for( auto& v : test )
            {
                if( strcmp( v, tmid ) == 0 )
                {
                    found = true;
                    break;
                }
            }
            if( !found )
            {
                return false;
            }
        }
    }

    return true;
}

bool Galaxy::AreParentsSame( uint32_t idx, const char* msgid ) const
{
    auto ptr = m_midgr[idx];
    auto num = *ptr++;

    std::vector<Archive*> arch;
    for( int i=0; i<num; i++ )
    {
        if( IsArchiveAvailable( *ptr ) )
        {
            arch.emplace_back( m_arch[*ptr].get() );
        }
        ptr++;
    }
    assert( !arch.empty() );
    if( arch.size() == 1 ) return true;

    const auto tid = arch[0]->GetParent( msgid );
    const auto tmid = tid == -1 ? "" : arch[0]->GetMessageId( tid );

    for( int i=1; i<arch.size(); i++ )
    {
        auto tid2 = arch[i]->GetParent( msgid );
        auto tmid2 = tid2 == -1 ? "" : arch[i]->GetMessageId( tid2 );
        if( strcmp( tmid, tmid2 ) != 0 )
        {
            return false;
        }
    }
    return true;
}

int32_t Galaxy::GetIndirectIndex( uint32_t idx ) const
{
    auto size = m_indirectDense.DataSize();
    const uint64_t* begin = m_indirectDense;
    auto end = begin + size;

    auto it = std::lower_bound( begin, end, idx );
    if( it == end || *it != idx )
    {
        return -1;
    }
    else
    {
        return it - begin;
    }
}

int Galaxy::ParentDepth( const char* msgid, uint32_t arch ) const
{
    int num = -1;
    auto idx = m_arch[arch]->GetMessageIndex( msgid );
    do
    {
        num++;
        idx = m_arch[arch]->GetParent( idx );
    }
    while( idx != -1 );
    return num;
}
