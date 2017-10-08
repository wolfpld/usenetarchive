#include <algorithm>

#include "../common/Filesystem.hpp"
#include "../common/TaskDispatch.hpp"
#include "../common/System.hpp"

#include "Galaxy.hpp"

Galaxy* Galaxy::Open( const std::string& fn )
{
    if( !Exists( fn ) || IsFile( fn ) ) return nullptr;

    auto base = fn;
    if( fn.back() != '/' ) base += '/';
    if( !Exists( base + "archives" ) || !Exists( base + "archives.meta" ) ||
        !Exists( base + "midgr" ) || !Exists( base + "midgr.meta" ) ||
        !Exists( base + "midhash" ) || !Exists( base + "midhash.meta" ) ||
        !Exists( base + "msgid" ) || !Exists( base + "msgid.meta" ) || !Exists( base + "msgid.codebook" ) ||
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
    , m_compress( fn + "msgid.codebook" )
    , m_arch( m_archives.Size() / 2 )
{
    const auto size = m_archives.Size() / 2;
    m_available.reserve( size );
    const auto cpus = System::CPUCores();
    TaskDispatch td( cpus );
    std::mutex lock;
    for( int i=0; i<size; i++ )
    {
        td.Queue( [this, i, &lock] {
            const auto path = std::string( m_archives[i*2], m_archives[i*2+1] );
            if( Exists( path ) )
            {
                auto arch = Archive::Open( path );
                if( arch )
                {
                    m_arch[i].reset( arch );
                    std::lock_guard<std::mutex> lg( lock );
                    m_available.emplace_back( i );
                }
            }
            else
            {
                const auto relative = m_base + path;
                auto arch = Archive::Open( relative );
                if( arch )
                {
                    m_arch[i].reset( arch );
                    std::lock_guard<std::mutex> lg( lock );
                    m_available.emplace_back( i );
                }
            }
        } );
    }
    td.Sync();
}

const std::shared_ptr<Archive>& Galaxy::GetArchive( int idx, bool change )
{
    if( change )
    {
        m_active = idx;
    }
    return m_arch[idx];
}

bool Galaxy::AreChildrenSame( uint32_t idx, const uint8_t* msgid ) const
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

    uint8_t local[2048];
    arch[0]->RepackMsgId( msgid, local, m_compress );
    const auto children = arch[0]->GetChildren( local );
    std::vector<ViewReference<uint32_t>> cvec;
    cvec.reserve( arch.size() - 1 );
    for( int i=1; i<arch.size(); i++ )
    {
        arch[i]->RepackMsgId( msgid, local, m_compress );
        const auto c = arch[i]->GetChildren( local );
        if( c.size != children.size )
        {
            return false;
        }
        cvec.emplace_back( c );
    }

    std::vector<const uint8_t*> test;
    test.reserve( children.size );
    for( int i=0; i<children.size; i++ )
    {
        test.emplace_back( arch[0]->GetMessageId( children.ptr[i] ) );
    }

    for( int i=1; i<arch.size(); i++ )
    {
        const auto& c2 = cvec[i-1];
        for( int j=0; j<c2.size; j++ )
        {
            auto tmid = arch[i]->GetMessageId( c2.ptr[j] );
            arch[0]->RepackMsgId( tmid, local, arch[i]->GetCompress() );

            bool found = false;
            for( auto& v : test )
            {
                if( strcmp( (const char*)v, (const char*)local ) == 0 )
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

bool Galaxy::AreParentsSame( uint32_t idx, const uint8_t* msgid ) const
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

    uint8_t local[2048];
    arch[0]->RepackMsgId( msgid, local, m_compress );
    const auto tid = arch[0]->GetParent( local );

    if( tid == -1 )
    {
        for( int i=1; i<arch.size(); i++ )
        {
            arch[i]->RepackMsgId( msgid, local, m_compress );
            auto tid2 = arch[i]->GetParent( local );
            if( tid2 != -1 )
            {
                return false;
            }
        }
    }
    else
    {
        const auto tmid = arch[0]->GetMessageId( tid );
        for( int i=1; i<arch.size(); i++ )
        {
            arch[i]->RepackMsgId( msgid, local, m_compress );
            auto tid2 = arch[i]->GetParent( local );
            if( tid2 == -1 )
            {
                return false;
            }
            auto tmid2 = arch[i]->GetMessageId( tid2 );
            uint8_t tlocal[2048];
            arch[i]->RepackMsgId( tmid, tlocal, arch[0]->GetCompress() );
            if( strcmp( (const char*)tlocal, (const char*)tmid2 ) != 0 )
            {
                return false;
            }
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

int Galaxy::ParentDepth( const uint8_t* msgid, uint32_t arch ) const
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

int Galaxy::NumberOfChildren( const uint8_t* msgid, uint32_t arch ) const
{
    return m_arch[arch]->GetChildren( msgid ).size;
}

int Galaxy::TotalNumberOfChildren( const uint8_t* msgid, uint32_t arch ) const
{
    return m_arch[arch]->GetTotalChildrenCount( msgid );
}