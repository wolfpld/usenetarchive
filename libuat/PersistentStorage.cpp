#include <ctype.h>
#include <stdlib.h>
#include <sstream>
#include <mutex>

#include "../common/FileMap.hpp"
#include "../common/Filesystem.hpp"
#include "../common/String.hpp"

#include "PersistentStorage.hpp"
#include "Score.hpp"

enum { BufSize = 1024 * 1024 };

static const char* LastOpenArchive = "lastopen";
static const char* LastOpenGalaxyArchive = "lastgalaxy";
static const char* LastArticle = "article-";
static const char* Visited = "visited";
static const char* Score = "score";

static std::string GetSavePath()
{
#if defined _MSC_VER || defined __CYGWIN__ || defined __MINGW32__
    std::string path( getenv( "APPDATA" ) );
    for( unsigned int i=0; i<path.size(); i++ )
    {
        if( path[i] == '\\' )
        {
            path[i] = '/';
        }
    }
    path += "/UsenetArchive/";
#else
    std::string path;
    const char* env = getenv( "XDG_CONFIG_HOME" );
    if( env && *env )
    {
        path = env;
        path += "/UsenetArchive/";
    }
    else
    {
        env = getenv( "HOME" );
        assert( env && *env );
        path = env;
        path += "/.config/UsenetArchive/";
    }
#endif
    return path;
}

PersistentStorage::PersistentStorage()
    : m_base( GetSavePath() )
    , m_visitedTimestamp( 0 )
    , m_visitedGuard( ( m_base + Visited ).c_str() )
    , m_currBuf( nullptr )
    , m_bufLeft( 0 )
    , m_articleHistory( 256 )
{
    LoadScore();
}

PersistentStorage::~PersistentStorage()
{
    for( auto& buf : m_buffers )
    {
        delete[] buf;
    }
}

void PersistentStorage::WriteLastOpenArchive( const char* archive )
{
    CreateDirStruct( m_base );
    LockedFile guard( ( m_base + LastOpenArchive ).c_str() );
    std::lock_guard<LockedFile> lg( guard );
    FILE* f = fopen( guard, "wb" );
    if( !f ) return;
    fwrite( archive, 1, strlen( archive ), f );
    fclose( f );
}

std::string PersistentStorage::ReadLastOpenArchive()
{
    std::string ret;
    const auto fn = m_base + LastOpenArchive;
    if( !Exists( fn ) ) return ret;
    LockedFile guard( fn.c_str() );
    std::lock_guard<LockedFile> lg( guard );
    FileMap<char> map( fn );
    ret.assign( (const char*)map, map + map.Size() );
    return ret;
}

void PersistentStorage::WriteLastOpenGalaxyArchive( uint32_t idx )
{
    CreateDirStruct( m_base );
    LockedFile guard( ( m_base + LastOpenGalaxyArchive ).c_str() );
    std::lock_guard<LockedFile> lg( guard );
    FILE* f = fopen( guard, "wb" );
    if( !f ) return;
    fwrite( &idx, 1, sizeof( idx ), f );
    fclose( f );
}

int32_t PersistentStorage::ReadLastOpenGalaxyArchive()
{
    int32_t ret = -1;
    const auto fn = m_base + LastOpenGalaxyArchive;
    if( !Exists( fn ) ) return ret;
    LockedFile guard( fn.c_str() );
    std::lock_guard<LockedFile> lg( guard );
    FILE* f = fopen( guard, "rb" );
    if( !f ) return ret;
    fread( &ret, 1, sizeof( ret ), f );
    fclose( f );
    return ret;
}

void PersistentStorage::AddToHistory( uint32_t idx )
{
    m_articleHistory.push_back( idx );
}

std::string PersistentStorage::CreateLastArticleFilename( const char* archive )
{
    std::ostringstream ss;
    ss << m_base << LastArticle;
    const auto size = strlen( archive );
    for( int i=0; i<size; i++ )
    {
        switch( archive[i] )
        {
        case '\\':
        case '/':
        case ':':
        case '*':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
            ss << '$';
            break;
        default:
            ss << archive[i];
            break;
        }
    }
    return ss.str();
}

void PersistentStorage::WriteArticleHistory( const char* archive )
{
    CreateDirStruct( m_base );
    const auto fn = CreateLastArticleFilename( archive );
    LockedFile guard( fn.c_str() );
    std::lock_guard<LockedFile> lg( guard );
    FILE* f = fopen( guard, "wb" );
    if( !f ) return;
    for( auto& v : m_articleHistory )
    {
        fwrite( &v, 1, sizeof( v ), f );
    }
    fclose( f );
}

bool PersistentStorage::ReadArticleHistory( const char* archive )
{
    m_articleHistory.clear();
    const auto fn = CreateLastArticleFilename( archive );
    if( !Exists( fn ) ) return false;
    LockedFile guard( fn.c_str() );
    std::lock_guard<LockedFile> lg( guard );
    FILE* f = fopen( guard, "rb" );
    if( !f ) return false;
    uint32_t tmp;
    const auto size = GetFileSize( guard ) / sizeof( tmp );
    for( int i=0; i<size; i++ )
    {
        fread( &tmp, 1, sizeof( tmp ), f );
        m_articleHistory.push_back( tmp );
    }
    return size != 0;
}

bool PersistentStorage::WasVisited( const char* msgid )
{
    if( std::chrono::duration_cast<std::chrono::milliseconds>( std::chrono::steady_clock::now() - m_visitedLastVerify ).count() < 500 )
    {
        return m_visited.find( msgid ) != m_visited.end();
    }
    if( m_visited.find( msgid ) != m_visited.end() ) return true;
    std::lock_guard<LockedFile> lg( m_visitedGuard );
    VerifyVisitedAreValid( m_visitedGuard );
    return m_visited.find( msgid ) != m_visited.end();
}

bool PersistentStorage::MarkVisited( const char* msgid )
{
    if( m_visited.find( msgid ) != m_visited.end() ) return false;
    std::lock_guard<LockedFile> lg( m_visitedGuard );
    VerifyVisitedAreValid( m_visitedGuard );
    m_visited.emplace( StoreString( msgid ) );
    CreateDirStruct( m_base );
    FILE* f = fopen( m_visitedGuard, "ab" );
    if( f )
    {
        fwrite( msgid, 1, strlen( msgid ) + 1, f );
        fclose( f );
        m_visitedTimestamp = GetFileMTime( m_visitedGuard );
    }
    return true;
}

const char* PersistentStorage::StoreString( const char* str )
{
    const auto size = strlen( str ) + 1;
    if( size > m_bufLeft )
    {
        m_currBuf = new char[BufSize];
        m_bufLeft = BufSize;
        m_buffers.emplace_back( m_currBuf );
    }
    memcpy( m_currBuf, str, size );
    const auto ret = m_currBuf;
    m_currBuf += size;
    m_bufLeft -= size;
    return ret;
}

void PersistentStorage::VerifyVisitedAreValid( const std::string& fn )
{
    if( !Exists( fn ) ) return;

    const auto timestamp = GetFileMTime( fn.c_str() );
    if( timestamp > m_visitedTimestamp )
    {
        m_visitedTimestamp = timestamp;
        FileMap<char> fmap( fn );
        auto ptr = (const char*)fmap;
        auto datasize = fmap.Size();
        while( datasize > 0 )
        {
            const auto size = strlen( ptr );
            if( m_visited.find( ptr ) == m_visited.end() )
            {
                m_visited.emplace( StoreString( ptr ) );
            }
            ptr += size + 1;
            datasize -= size + 1;
        }
    }
    m_visitedLastVerify = std::chrono::steady_clock::now();
}

void PersistentStorage::LoadScore()
{
    const auto fn = m_base + Score;
    if( !Exists( fn ) ) return;

    FileMap<char> f( fn );
    auto size = f.DataSize();
    const char* ptr = f;
    while( size > 0 )
    {
        int score;
        if( sscanf( ptr, "%i", &score ) != 1 ) return;
        while( size > 0 && !isspace( *ptr ) )
        {
            ptr++;
            size--;
        }
        while( size > 0 && isspace( *ptr ) )
        {
            ptr++;
            size--;
        }
        if( size <= 0 ) return;
        ScoreField field;
        if( size > 8 && strnicmpl( ptr, "realname", 8 ) == 0 )
        {
            field = SF_RealName;
            ptr += 8;
            size -= 8;
        }
        else if( size > 7 && strnicmpl( ptr, "subject", 7 ) == 0 )
        {
            field = SF_Subject;
            ptr += 7;
            size -= 7;
        }
        else if( size > 4 && strnicmpl( ptr, "from", 4 ) == 0 )
        {
            field = SF_Subject;
            ptr += 4;
            size -= 4;
        }
        else
        {
            return;
        }
        while( size && isspace( *ptr ) )
        {
            ptr++;
            size--;
        }
        if( size <= 0 ) return;

        bool exact, ignoreCase;
        switch( *ptr )
        {
        case '=':
            exact = true;
            ignoreCase = false;
            break;
        case '~':
            exact = true;
            ignoreCase = true;
            break;
        case '!':
            exact = false;
            ignoreCase = false;
            break;
        case '?':
            exact = false;
            ignoreCase = true;
            break;
        default:
            return;
        }

        ptr++;
        size--;
        if( size <= 0 ) return;

        auto end = ptr;
        while( end - ptr < size && *end != '\n' && *end != '\r' ) end++;
        std::string match( ptr, end );
        ptr = end;
        while( size > 0 && *ptr == '\n' || *ptr == '\r' )
        {
            ptr++;
            size--;
        }

        m_scoreList.emplace_back( ScoreEntry { score, exact, ignoreCase, field, std::move( match ) } );
    }
}
