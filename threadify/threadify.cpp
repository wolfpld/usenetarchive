#include <assert.h>
#include <algorithm>
#include <ctype.h>
#include <limits>
#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

#ifdef _WIN32
#  include <malloc.h>
#else
#  include <alloca.h>
#endif

#include <unicode/locid.h>
#include <unicode/brkiter.h>
#include <unicode/unistr.h>

#include "../libuat/Archive.hpp"
#include "../common/String.hpp"

UErrorCode wordItErr = U_ZERO_ERROR;
auto wordIt = icu::BreakIterator::createWordInstance( icu::Locale::getEnglish(), wordItErr );

void SplitLine( const char* ptr, const char* end, std::vector<std::string>& out )
{
    out.clear();
    auto us = icu::UnicodeString::fromUTF8( StringPiece( ptr, end-ptr ) );
    auto lower = us.toLower( icu::Locale::getEnglish() );

    wordIt->setText( lower );
    int32_t p0 = 0;
    int32_t p1 = wordIt->first();
    while( p1 != icu::BreakIterator::DONE )
    {
        auto part = lower.tempSubStringBetween( p0, p1 );
        std::string str;
        part.toUTF8String( str );
        if( str.size() > 2 && str.size() < 14 )
        {
            out.emplace_back( std::move( str ) );
        }
        p0 = p1;
        p1 = wordIt->next();
    }
}

struct Message
{
    uint32_t epoch = 0;
    int32_t parent = -1;
    std::vector<uint32_t> children;
};

void Sort( std::vector<uint32_t>& vec, const Message* msg )
{
    std::sort( vec.begin(), vec.end(), [msg]( const uint32_t l, const uint32_t r ) { return msg[l].epoch < msg[r].epoch; } );
}

static const char* ReList[] = {
    "Re:",
    "RE:",
    "re:",
    "Odp:",
    nullptr
};

const char* KillRe( const char* str )
{
    for(;;)
    {
        if( *str == '\0' ) return str;
        while( *str == ' ' ) str++;
        auto match = ReList;
        bool stop = false;
        while( !stop )
        {
            int idx = 0;
            auto matchstr = *match;
            for(;;)
            {
                if( matchstr[idx] == '\0' )
                {
                    str += idx;
                    stop = true;
                    break;
                }
                if( str[idx] != matchstr[idx] ) break;
                idx++;
            }
            if( !stop )
            {
                match++;
                if( !*match )
                {
                    return str;
                }
            }
        }
    }
}

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s raw\n", argv[0] );
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
                printf( "%i/%i\r", i, size );
                fflush( stdout );
            }
            toplevel.push_back( data[i] );
        }
    }

    printf( "\nReading connectivity data...\n" );
    fflush( stdout );
    auto msgdata = new Message[size];
    {
        MetaView<uint32_t, uint32_t> conn( base + "connmeta", base + "conndata" );
        for( int i=0; i<size; i++ )
        {
            if( ( i & 0x3FF ) == 0 )
            {
                printf( "%i/%i\r", i, size );
                fflush( stdout );
            }
            auto ptr = conn[i];
            msgdata[i].epoch = *ptr++;
            msgdata[i].parent = *ptr++;
            uint32_t cnum = *ptr++;
            msgdata[i].children.reserve( cnum );
            for( int j=0; j<cnum; j++ )
            {
                msgdata[i].children.push_back( *ptr++ );
            }
        }
    }

    printf( "\nMatching messages...\n" );

    std::vector<std::string> wordbuf;
    int cntrefs = 0, cntnew = 0, cntsure = 0, cntgood = 0, cntbad = 0;
    std::vector<std::pair<uint32_t, uint32_t>> found;
    std::map<uint32_t, float> hits;

    for( int j=0; j<toplevel.size(); j++ )
    {
        if( ( j & 0x1F ) == 0 )
        {
            printf( "%i/%i\r", j, toplevel.size() );
            fflush( stdout );
        }

        auto i = toplevel[j];
        bool headers = true;

        auto post = archive->GetMessage( i );
        for(;;)
        {
            auto end = post;
            if( headers )
            {
                if( strnicmpl( post, "references: ", 12 ) == 0 || strnicmpl( post, "in-reply-to: ", 13 ) == 0 )
                {
                    cntrefs++;
                    break;
                }
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
                int quotLevel = 0;
                while( *end != '\n' && *end != '\0' ) end++;
                while( *line == ' ' || *line == '>' || *line == ':' || *line == '|' || *line == '\t' )
                {
                    if( *line == '>' || *line == ':' || *line == '|' )
                    {
                        quotLevel++;
                    }
                    line++;
                }
                if( line != end && quotLevel == 1 )
                {
                    SplitLine( line, end, wordbuf );
                    if( !wordbuf.empty() )
                    {
                        auto res = archive->Search( wordbuf, T_Content );
                        if( !res.empty() )
                        {
                            for( auto r : res )
                            {
                                hits[r.postid] += r.rank;
                            }
                        }
                        wordbuf.clear();
                    }
                }
                if( *end == '\0' ) break;
                post = end + 1;
            }
        }
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
            if( i == best )
            {
                cntnew++;
            }
            else
            {
                auto s1 = KillRe( archive->GetSubject( i ) );
                auto s2 = KillRe( archive->GetSubject( best ) );
                if( strcmp( s1, s2 ) == 0 )
                {
                    cntsure++;
                    found.emplace_back( i, best );
                }
                else
                {
                    cntbad++;
                }
            }
        }
    }

    printf( "\nApplying changes...\n" );
    fflush( stdout );
    for( auto& v : found )
    {
        msgdata[v.first].parent = v.second;
        msgdata[v.second].children.push_back( v.first );

        Sort( msgdata[v.second].children, msgdata );

        auto it = std::find( toplevel.begin(), toplevel.end(), v.first );
        assert( it != toplevel.end() );
        toplevel.erase( it );
    }

    archive.reset();

    if( !found.empty() )
    {
        printf( "Saving...\n" );
        printf( "WARNING! Lexicon data has been invalidated!\n" );

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
                printf( "%i/%i\r", i, size );
                fflush( stdout );
            }

            fwrite( &offset, 1, sizeof( uint32_t ), cmeta );

            offset += fwrite( &msgdata[i].epoch, 1, sizeof( Message::epoch ), cdata );
            offset += fwrite( &msgdata[i].parent, 1, sizeof( Message::parent ), cdata );
            uint32_t cnum = msgdata[i].children.size();
            offset += fwrite( &cnum, 1, sizeof( cnum ), cdata );
            for( auto& v : msgdata[i].children )
            {
                offset += fwrite( &v, 1, sizeof( v ), cdata );
            }
        }
        fclose( cdata );
        fclose( cmeta );
    }

    printf( "\nFound %i new threads, %i broken threads (bad references).\nSurely matched %i messages (same thread line), also %i good and %i guesses.\n", cntnew, cntrefs, cntsure, cntgood, cntbad );

    return 0;
}
