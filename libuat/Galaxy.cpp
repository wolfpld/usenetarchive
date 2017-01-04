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

std::string Galaxy::GetArchiveFilename( int idx ) const
{
    return std::string( m_archives[idx*2], m_archives[idx*2+1] );
}

bool Galaxy::IsArchiveAvailable( int idx ) const
{
    return *std::lower_bound( m_available.begin(), m_available.end(), idx ) == idx;
}

const std::shared_ptr<Archive>& Galaxy::GetArchive( int idx )
{
    m_active = idx;
    return m_arch[idx];
}

const char* Galaxy::GetArchiveName( int idx ) const
{
    return m_strings[idx*2];
}

const char* Galaxy::GetArchiveDescription( int idx ) const
{
    return m_strings[idx*2+1];
}

int Galaxy::NumberOfMessages( int idx ) const
{
    return m_arch[idx]->NumberOfMessages();
}

int Galaxy::NumberOfTopLevel( int idx ) const
{
    return m_arch[idx]->NumberOfTopLevel();
}

int Galaxy::GetMessageIndex( const char* msgid ) const
{
    return m_midhash.Search( msgid );
}

int Galaxy::GetNumberOfGroups( const char* msgid ) const
{
    return GetNumberOfGroups( GetMessageIndex( msgid ) );
}

int Galaxy::GetNumberOfGroups( uint32_t idx ) const
{
    if( idx == -1 ) return 0;
    auto ptr = m_midgr[idx];
    return *ptr;
}

bool Galaxy::AreChildrenSame( const char* msgid ) const
{
    auto idx = GetMessageIndex( msgid );
    assert( idx != -1 );
    return AreChildrenSame( idx, msgid );
}

bool Galaxy::AreChildrenSame( uint32_t idx, const char* msgid ) const
{
    auto ptr = m_midgr[idx];
    auto num = *ptr++;

    std::vector<Archive*> arch;
    for( int i=0; i<num; i++ )
    {
        if( *std::lower_bound( m_available.begin(), m_available.end(), *ptr ) == *ptr )
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

bool Galaxy::AreParentsSame( const char* msgid ) const
{
    auto idx = GetMessageIndex( msgid );
    assert( idx != -1 );
    return AreParentsSame( idx, msgid );
}

bool Galaxy::AreParentsSame( uint32_t idx, const char* msgid ) const
{
    auto ptr = m_midgr[idx];
    auto num = *ptr++;

    std::vector<Archive*> arch;
    for( int i=0; i<num; i++ )
    {
        if( *std::lower_bound( m_available.begin(), m_available.end(), *ptr ) == *ptr )
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

ViewReference<uint32_t> Galaxy::GetGroups( const char* msgid ) const
{
    auto idx = GetMessageIndex( msgid );
    assert( idx != -1 );
    auto ptr = m_midgr[idx];
    auto num = *ptr++;
    return ViewReference<uint32_t> { ptr, num };
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

int Galaxy::NumberOfChildren( const char* msgid, uint32_t arch ) const
{
    return m_arch[arch]->GetChildren( msgid ).size;
}

int Galaxy::TotalNumberOfChildren( const char* msgid, uint32_t arch ) const
{
    return m_arch[arch]->GetTotalChildrenCount( msgid );
}
