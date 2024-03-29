#include <assert.h>
#include <algorithm>
#include <atomic>
#include <ctype.h>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "../libuat/Archive.hpp"
#include "../libuat/SearchEngine.hpp"
#include "../common/ExpandingBuffer.hpp"
#include "../common/ICU.hpp"
#include "../common/KillRe.hpp"
#include "../common/MessageLogic.hpp"
#include "../common/ReferencesParent.hpp"
#include "../common/System.hpp"
#include "../common/TaskDispatch.hpp"
#include "../contrib/martinus/robin_hood.h"

struct Message
{
    uint32_t epoch = 0;
    int32_t parent = -1;
    uint32_t childTotal = 0;
    std::vector<uint32_t> children;
};

void Sort( std::vector<uint32_t>& vec, const Message* msg )
{
    std::sort( vec.begin(), vec.end(), [msg]( const uint32_t l, const uint32_t r ) { return msg[l].epoch < msg[r].epoch; } );
}

uint32_t* root;
Message* msgdata;

void SetRootTo( uint32_t idx, uint32_t val )
{
    root[idx] = val;
    for( auto& v : msgdata[idx].children )
    {
        SetRootTo( v, val );
    }
}

static size_t RemoveSpaces( const char* in, char* out )
{
    const auto refout = out;
    while( *in != '\0' )
    {
        if( *in != ' ' && *in != '\t' )
        {
            *out++ = *in;
        }
        in++;
    }
    *out++ = '\0';
    return out - refout;
}

// s1 is child
// s2 is parent
static bool IsSubjectMatch( const char* s1, const char* s2, const KillRe& kill )
{
    auto k1 = kill.Kill( s1 );
    auto k2 = kill.Kill( s2 );
    if( strcmp( k1, k2 ) == 0 ) return true;

    if( strlen( k1 ) >= 1024 || strlen( k2 ) >= 1024 ) return false;

    char r1[1024];
    char r2[1024];
    const auto sz1 = RemoveSpaces( k1, r1 );
    const auto sz2 = RemoveSpaces( k2, r2 );

    return sz1 == sz2 && memcmp( r1, r2, sz1 ) == 0;
}

template<class T>
static bool HasUniqueElements( std::vector<T>& v )
{
    std::sort( v.begin(), v.end() );
    return std::adjacent_find( v.begin(), v.end() ) == v.end();
}


int main( int argc, char** argv )
{
    if( argc < 2 || ( argc != 3 && ( argc % 2 ) == 1 ) )
    {
        fprintf( stderr, "USAGE: %s raw ( [-i ignore]* | [-g] )\n", argv[0] );
        fprintf( stderr, "  -i: add string to re:-list filter\n" );
        fprintf( stderr, "  -g: group threads by scanning for missing references\n" );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );

    std::unique_ptr<Archive> archive( Archive::Open( argv[1] ) );
    if( !archive )
    {
        fprintf( stderr, "Cannot open %s\n", argv[1] );
        return 1;
    }
    auto size = archive->NumberOfMessages();
    const SearchEngine search( *archive );
    KillRe kr;
    kr.LoadPrefixList( *archive );

    bool groupMode = false;

    while( argc > 2 )
    {
        if( strcmp( argv[2], "-i" ) == 0 )
        {
            kr.Add( argv[3] );
            argv += 2;
            argc -= 2;
        }
        else if( strcmp( argv[2], "-g" ) == 0 )
        {
            groupMode = true;
            argv++;
            argc--;
        }
        else
        {
            fprintf( stderr, "Bad params!\n" );
            exit( 1 );
        }
    }

    printf( "Reading toplevel data...\n" );
    fflush( stdout );
    std::vector<uint32_t> toplevel;
    {
        FileMap<uint32_t> data( base + "toplevel" );
        auto size = data.DataSize();
        toplevel.reserve( size );
        for( int i=0; i<size; i++ )
        {
            if( ( i & 0xFFF ) == 0 )
            {
                printf( "%i/%zu\r", i, size );
                fflush( stdout );
            }
            toplevel.push_back( data[i] );
        }
    }

    printf( "\nReading connectivity data...\n" );
    fflush( stdout );
    msgdata = new Message[size];
    {
        MetaView<uint32_t, uint32_t> conn( base + "connmeta", base + "conndata" );
        for( int i=0; i<size; i++ )
        {
            if( ( i & 0x3FF ) == 0 )
            {
                printf( "%i/%zu\r", i, size );
                fflush( stdout );
            }
            auto ptr = conn[i];
            msgdata[i].epoch = *ptr++;
            msgdata[i].parent = *ptr++;
            msgdata[i].childTotal = *ptr++;
            uint32_t cnum = *ptr++;
            msgdata[i].children.reserve( cnum );
            for( int j=0; j<cnum; j++ )
            {
                msgdata[i].children.push_back( *ptr++ );
            }
        }
    }

    const auto topsize = toplevel.size();
    std::vector<std::pair<uint32_t, uint32_t>> found;
    int cntnew = 0, cntsure = 0, cntbad = 0, cnttime = 0;
    printf( "\nGrouping messages...\n" );

    if( groupMode )
    {
        struct Group
        {
            std::vector<uint32_t> msg;
        };

        ExpandingBuffer eb;
        robin_hood::unordered_flat_map<uint32_t, Group> groups;
        robin_hood::unordered_flat_map<std::string, uint32_t> refgroup;
        uint32_t curgroup = 0;

        for( int j=0; j<topsize; j++ )
        {
            if( ( j & 0x3FF ) == 0 )
            {
                printf( "%i/%zu\r", j, topsize );
                fflush( stdout );
            }

            auto i = toplevel[j];
            auto post = archive->GetMessage( i, eb );

            const auto refs = GetAllReferences( post, archive->GetCompress() );

            if( !refs.empty() )
            {
                std::set<uint32_t> merge;
                for( auto& ref : refs )
                {
                    auto it = refgroup.find( ref );
                    if( it != refgroup.end() )
                    {
                        merge.emplace( it->second );
                    }
                }
                if( merge.empty() )
                {
                    Group g;
                    g.msg.emplace_back( i );
                    groups.emplace( std::make_pair( curgroup, std::move( g ) ) );
                    for( auto& ref : refs )
                    {
                        refgroup[ref] = curgroup;
                    }
                    curgroup++;
                }
                else if( merge.size() == 1 )
                {
                    auto gidx = *merge.begin();
                    auto it = groups.find( gidx );
                    assert( it != groups.end() );
                    auto& g = it->second;
                    g.msg.emplace_back( i );
                    for( auto& ref : refs )
                    {
                        refgroup[ref] = gidx;
                    }
                }
                else
                {
                    auto mit = merge.begin();
                    auto gidx = *mit;
                    auto it = groups.find( gidx );
                    assert( it != groups.end() );
                    auto& g = it->second;
                    while( ++mit != merge.end() )
                    {
                        it = groups.find( *mit );
                        assert( it != groups.end() );
                        auto& sg = it->second;
                        for( auto m : sg.msg )
                        {
                            if( std::find( g.msg.begin(), g.msg.end(), m ) == g.msg.end() )
                            {
                                g.msg.emplace_back( m );
                            }
                        }
                        for( auto& ref : refgroup )
                        {
                            if( ref.second == it->first )
                            {
                                ref.second = gidx;
                            }
                        }
                        groups.erase( it );
                    }
                    g.msg.emplace_back( i );
                    assert( HasUniqueElements( g.msg ) );
                    for( auto& ref : refs )
                    {
                        refgroup[ref] = gidx;
                    }
                }
            }
        }
        for( auto& v : groups )
        {
            auto& g = v.second;
            if( g.msg.size() > 1 )
            {
                assert( HasUniqueElements( g.msg ) );
                std::sort( g.msg.begin(), g.msg.end(), [&archive] ( const auto& l, const auto& r ) { return archive->GetDate( l ) < archive->GetDate( r ); } );
                auto parent = g.msg[0];
                for( int i=1; i<g.msg.size(); i++ )
                {
                    found.emplace_back( g.msg[i], parent );
                    cntsure++;
                }
            }
        }
    }
    else
    {
        root = new uint32_t[size];
        for( int i=0; i<size; i++ )
        {
            if( ( i & 0x3FF ) == 0 )
            {
                printf( "%i/%zu\r", i, size );
                fflush( stdout );
            }

            auto idx = i;
            while( msgdata[idx].parent != -1 ) idx = msgdata[idx].parent;
            root[i] = idx;
        }

        printf( "\nMatching messages...\n" );

        const auto cpus = System::CPUCores();
        TaskDispatch tasks( cpus-1 );
        std::atomic<uint32_t> cnt( 0 );

        std::mutex viewLock, resLock, splitLock;

        for( int t=0; t<cpus; t++ )
        {
            tasks.Queue( [&cnt, &topsize, &toplevel, &viewLock, &resLock, &splitLock, &archive, &search, &found, &cntnew, &cntsure, &cntbad, &cnttime, &kr] {
                ExpandingBuffer eb;
                robin_hood::unordered_flat_map<uint32_t, float> hits;
                std::vector<std::string> wordbuf;

                for(;;)
                {
                    auto j = cnt.fetch_add( 1, std::memory_order_relaxed );
                    if( j >= topsize ) break;
                    if( ( j & 0x1F ) == 0 )
                    {
                        printf( "%i/%zu\r", j, topsize );
                        fflush( stdout );
                    }

                    auto i = toplevel[j];
                    bool headers = true;
                    bool wroteDone = false;
                    int remaining = 16;

                    viewLock.lock();
                    auto post = archive->GetMessage( i, eb );
                    viewLock.unlock();

                    for(;;)
                    {
                        auto end = post;
                        if( headers )
                        {
                            if( *end == '\n' )
                            {
                                headers = false;
                                continue;
                            }

                            while( *end != '\n' ) end++;
                            post = end + 1;
                        }
                        else
                        {
                            const char* line = end;
                            while( *end != '\n' && *end != '\0' ) end++;
                            int quotLevel = QuotationLevel( line, end );
                            if( line != end && quotLevel == 1 )
                            {
                                const char* wrote = line;
                                if( !wroteDone )
                                {
                                    wrote = DetectWroteEnd( line, 1 );
                                    wroteDone = true;
                                }
                                if( wrote == line )
                                {
                                    splitLock.lock();
                                    SplitLine( line, end, wordbuf );
                                    splitLock.unlock();
                                    if( !wordbuf.empty() )
                                    {
                                        auto results = search.Search( wordbuf, SearchEngine::SF_RequireAllWords | SearchEngine::SF_SimpleSearch, T_Content );
                                        auto& res = results.results;
                                        if( !res.empty() )
                                        {
                                            auto terminate = res[0].rank * 0.02;
                                            auto matched = results.matched.size();
                                            for( auto& r : res )
                                            {
                                                if( r.rank < terminate ) break;
                                                hits[r.postid] += r.rank * matched * matched;
                                            }
                                        }
                                        wordbuf.clear();
                                    }
                                    if( --remaining == 0 ) break;
                                }
                                else
                                {
                                    end = wrote;
                                    while( *end != '\n' ) end--;
                                }
                            }
                            if( *end == '\0' ) break;
                            post = end + 1;
                        }
                    }

                    resLock.lock();
                    if( hits.empty() )
                    {
                        cntnew++;
                    }
                    else
                    {
                        uint32_t best = 0;
                        float rank = 0;
                        for( auto& h : hits )
                        {
                            if( h.second > rank )
                            {
                                rank = h.second;
                                best = h.first;
                            }
                        }
                        hits.clear();
                        if( root[i] == root[best] )
                        {
                            cntnew++;
                        }
                        else
                        {
                            time_t t1 = archive->GetDate( i );
                            time_t t2 = archive->GetDate( best );
                            if( ( t1 > t2 + 60 * 60 * 24 * 365 ) ||     // child message is year+ younger than parent
                                ( t1 < t2 - 60 * 60 * 24 * 30 ) )       // child message is month+ older than parent
                            {
                                cnttime++;
                            }
                            else
                            {
                                if( IsSubjectMatch( archive->GetSubject( i ), archive->GetSubject( best ), kr ) )
                                {
                                    cntsure++;
                                    found.emplace_back( i, best );
                                    SetRootTo( i, root[best] );
                                }
                                else
                                {
                                    cntbad++;
                                }
                            }
                        }
                    }
                    resLock.unlock();
                }
            } );
        }
        tasks.Sync();
        std::sort( found.begin(), found.end(), [] ( const auto& l, const auto& r ) { return l.first < r.first; } );
    }
    printf( "%zu/%zu\n", topsize, topsize );

    robin_hood::unordered_flat_set<uint32_t> bad;

    printf( "Applying changes...\n" );
    fflush( stdout );
    for( auto& v : found )
    {
        if( bad.find( v.first ) != bad.end() ) continue;

        msgdata[v.first].parent = v.second;
        msgdata[v.second].children.push_back( v.first );

        auto add = msgdata[v.first].childTotal;
        auto idx = v.second;
        for(;;)
        {
            msgdata[idx].childTotal += add;
            auto parent = msgdata[idx].parent;
            if( parent == -1 )
            {
                bad.emplace( idx );
                break;
            }
            idx = parent;
        }

        if( !groupMode )
        {
            Sort( msgdata[v.second].children, msgdata );
        }

        auto it = std::find( toplevel.begin(), toplevel.end(), v.first );
        assert( it != toplevel.end() );
        toplevel.erase( it );
    }

    archive.reset();

    if( !found.empty() )
    {
        printf( "Saving...\n" );
        printf( "WARNING! Sorting order has been changed! Run sort and lexsort.\n" );

        FILE* tlout = fopen( ( base + "toplevel" ).c_str(), "wb" );
        fwrite( toplevel.data(), 1, sizeof( uint32_t ) * toplevel.size(), tlout );
        fclose( tlout );

        FILE* cdata = fopen( ( base + "conndata" ).c_str(), "wb" );
        FILE* cmeta = fopen( ( base + "connmeta" ).c_str(), "wb" );
        uint32_t offset = 0;
        for( uint32_t i=0; i<size; i++ )
        {
            if( ( i & 0x1FFF ) == 0 )
            {
                printf( "%i/%zu\r", i, size );
                fflush( stdout );
            }

            fwrite( &offset, 1, sizeof( uint32_t ), cmeta );

            offset += fwrite( &msgdata[i].epoch, 1, sizeof( Message::epoch ), cdata );
            offset += fwrite( &msgdata[i].parent, 1, sizeof( Message::parent ), cdata );
            offset += fwrite( &msgdata[i].childTotal, 1, sizeof( Message::childTotal ), cdata );
            uint32_t cnum = msgdata[i].children.size();
            offset += fwrite( &cnum, 1, sizeof( cnum ), cdata );
            for( auto& v : msgdata[i].children )
            {
                offset += fwrite( &v, 1, sizeof( v ), cdata );
            }
        }
        fclose( cdata );
        fclose( cmeta );

        size_t lexsize;
        LexiconDataPacket* lexdata;
        {
            FileMap<LexiconDataPacket> lex( base + "lexdata" );
            lexsize = lex.DataSize();
            lexdata = new LexiconDataPacket[lexsize];
            memcpy( lexdata, lex, lexsize * sizeof( LexiconDataPacket ) );
        }

        for( size_t i=0; i<lexsize; i++ )
        {
            const auto postid = lexdata[i].postid & LexiconPostMask;
            const auto children = LexiconTransformChildNum( msgdata[postid].childTotal - 1 );
            lexdata[i].postid = postid | ( children << LexiconChildShift );
        }

        FILE* flex = fopen( ( base + "lexdata" ).c_str(), "wb" );
        fwrite( lexdata, 1, lexsize * sizeof( LexiconDataPacket ), flex );
        fclose( flex );
    }

    printf( "\nFound %i new threads.\nSurely matched %i messages (same subject line). Wrong guesses: %i due to different subject + %i non-chronological\n", cntnew, cntsure, cntbad, cnttime );

    return cntsure != 0 ? 1 : 0;
}
