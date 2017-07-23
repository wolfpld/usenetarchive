#include <algorithm>
#include <array>
#include <assert.h>
#include <inttypes.h>
#include <limits>
#include <map>
#include <set>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../common/CharUtil.hpp"
#include "../common/ExpandingBuffer.hpp"
#include "../common/Filesystem.hpp"
#include "../common/FileMap.hpp"
#include "../common/HashSearchBig.hpp"
#include "../common/MetaView.hpp"
#include "../common/MessageLogic.hpp"
#include "../common/MsgIdHash.hpp"
#include "../common/StringCompress.hpp"

#include "../libuat/Archive.hpp"

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s directory\n", argv[0] );
        exit( 1 );
    }
    if( !Exists( argv[1] ) )
    {
        fprintf( stderr, "Destination directory doesn't exist.\n" );
        exit( 1 );
    }

    const auto base = std::string( argv[1] ) + "/";
    const auto listfn = base + "archives";
    if( !Exists( listfn ) )
    {
        fprintf( stderr, "Archive file list doesn't exist. Create %s with paths to each archive in separate lines.\n", listfn.c_str() );
        exit( 1 );
    }

    std::vector<std::string> archives;

    // load archive list
    {
        FileMap<char> listfile( listfn );
        const char* begin = listfile;
        auto ptr = begin;
        auto size = listfile.DataSize();

        FILE* out = fopen( ( base + "archives.meta" ).c_str(), "wb" );

        auto end = ptr;
        while( size > 0 )
        {
            while( size > 0 && *end != '\r' && *end != '\n' )
            {
                end++;
                size--;
            }
            archives.emplace_back( ptr, end );

            if( !Exists( archives.back() ) )
            {
                fprintf( stderr, "Archive doesn't exist: %s\n", archives.back().c_str() );
                fclose( out );
                exit( 1 );
            }

            uint32_t tmp;
            tmp = ptr - begin;
            fwrite( &tmp, 1, sizeof( tmp ), out );
            tmp = end - begin;
            fwrite( &tmp, 1, sizeof( tmp ), out );

            while( size > 0 && ( *end == '\r' || *end == '\n' ) )
            {
                end++;
                size--;
            }
            ptr = end;
        }
        fclose( out );
    }

    int i = 0;
    uint64_t count = 0;
    std::vector<std::unique_ptr<Archive>> arch;
    for( auto& v : archives )
    {
        printf( "%i/%i\r", ++i, archives.size() );
        fflush( stdout );
        auto ptr = Archive::Open( v );
        if( !ptr )
        {
            fprintf( stderr, "Cannot open archive: %s\n", v.c_str() );
            exit( 1 );
        }
        arch.emplace_back( ptr );
        count += ptr->NumberOfMessages();
    }
    printf( "\nTotal message count: %" PRIu64 "\n", count );

    // name, description
    {
        FILE* data = fopen( ( base + "str" ).c_str(), "wb" );
        FILE* meta = fopen( ( base + "str.meta" ).c_str(), "wb" );

        uint32_t zero = 0;
        uint32_t offset = fwrite( &zero, 1, 1, data );

        int i=0;
        for( auto& v : arch )
        {
            auto name = v->GetArchiveName();
            fwrite( &offset, 1, sizeof( offset ), meta );
            if( name.second == 0 )
            {
                auto pos = archives[i].rfind( '/' );
                if( pos == std::string::npos )
                {
                    auto pos = archives[i].rfind( '\\' );
                }
                if( pos == std::string::npos )
                {
                    offset += fwrite( archives[i].c_str(), 1, archives[i].size() + 1, data );
                }
                else
                {
                    offset += fwrite( archives[i].c_str() + pos, 1, archives[i].size() + 1 - pos, data );
                }
            }
            else
            {
                offset += fwrite( name.first, 1, name.second, data );
                offset += fwrite( &zero, 1, 1, data );
            }

            auto desc = v->GetShortDescription();
            if( desc.second == 0 )
            {
                fwrite( &zero, 1, sizeof( zero ), meta );
            }
            else
            {
                fwrite( &offset, 1, sizeof( offset ), meta );
                offset += fwrite( desc.first, 1, desc.second, data );
                offset += fwrite( &zero, 1, 1, data );
            }

            i++;
        }

        fclose( data );
        fclose( meta );
    }

    // list of unique msg id
    StringCompress* compress;

    std::vector<const uint8_t*> msgidvec;
    uint64_t unique;
    {
        uint64_t cnt = 0;
        std::set<const char*, CharUtil::LessComparator> msgidset;
        for( int i=0; i<arch.size(); i++ )
        {
            const auto& a = *arch[i];
            const auto num = a.NumberOfMessages();
            for( size_t j=0; j<num; j++ )
            {
                if( ( cnt++ & 0x3FFF ) == 0 )
                {
                    printf( "%i/%i\r", cnt, count );
                    fflush( stdout );
                }

                const auto id = a.GetMessageId( j );
                auto it = msgidset.find( id );
                if( it == msgidset.end() )
                {
                    msgidset.emplace( id );
                }
            }
        }
        unique = msgidset.size();
        printf( "\nUnique message count: %zu\n", unique );

        printf( "Building code book...\n" );
        fflush( stdout );
        compress = new StringCompress( msgidset );
        compress->WriteData( base + "msgid.codebook" );

        printf( "Packing msg ids\n" );
        msgidvec.reserve( unique );
        cnt = 0;
        for( auto& v : msgidset )
        {
            if( ( cnt++ & 0x3FFF ) == 0 )
            {
                printf( "%i/%i\r", cnt, unique );
                fflush( stdout );
            }

            uint8_t tmp[2048];
            compress->Pack( v, tmp );
            msgidvec.emplace_back( (const uint8_t*)strdup( (const char*)tmp ) );
        }
        printf( "\n" );
    }

    // create hash table
    {
        auto hashbits = MsgIdHashBits( unique, 90 );
        auto hashsize = MsgIdHashSize( hashbits );
        auto hashmask = MsgIdHashMask( hashbits );

        printf( "Load factor: %.2f\n", float( unique ) / hashsize );

        auto hashdata = new uint64_t[hashsize];
        auto distance = new uint8_t[hashsize];
        memset( distance, 0xFF, hashsize );
        uint8_t distmax = 0;

        for( int i=0; i<unique; i++ )
        {
            if( ( i & 0x3FFFF ) == 0 )
            {
                printf( "%i/%i\r", i, unique );
                fflush( stdout );
            }

            uint32_t hash = XXH32( msgidvec[i], strlen( (const char*)msgidvec[i] ), 0 ) & hashmask;

            uint8_t dist = 0;
            uint64_t idx = i;
            for(;;)
            {
                if( distance[hash] == 0xFF )
                {
                    if( distmax < dist ) distmax = dist;
                    distance[hash] = dist;
                    hashdata[hash] = idx;
                    break;
                }
                if( distance[hash] < dist )
                {
                    if( distmax < dist ) distmax = dist;
                    std::swap( distance[hash], dist );
                    std::swap( hashdata[hash], idx );
                }
                dist++;
                assert( dist < 0xFF );
                hash = (hash+1) & hashmask;
            }
        }
        printf( "\n" );

        {
            FILE* meta = fopen( ( base + "midhash.meta" ).c_str(), "wb" );
            fwrite( &distmax, 1, 1, meta );
            fclose( meta );

            FILE* data = fopen( ( base + "midhash" ).c_str(), "wb" );
            FILE* strdata = fopen( ( base + "msgid" ).c_str(), "wb" );
            FILE* strmeta = fopen( ( base + "msgid.meta" ).c_str(), "wb" );

            const uint64_t zero = 0;
            uint64_t stroffset = fwrite( &zero, 1, 1, strdata );

            auto msgidoffset = new uint64_t[unique];

            int cnt = 0;
            for( int i=0; i<hashsize; i++ )
            {
                if( ( i & 0x3FFFF ) == 0 )
                {
                    printf( "%i/%i\r", i, hashsize );
                    fflush( stdout );
                }

                if( distance[i] == 0xFF )
                {
                    fwrite( &zero, 1, sizeof( uint64_t ), data );
                    fwrite( &zero, 1, sizeof( uint64_t ), data );
                }
                else
                {
                    fwrite( &stroffset, 1, sizeof( uint64_t ), data );
                    fwrite( hashdata+i, 1, sizeof( uint64_t ), data );

                    msgidoffset[hashdata[i]] = stroffset;
                    cnt++;
                    auto str = msgidvec[hashdata[i]];
                    stroffset += fwrite( str, 1, strlen( (const char*)str ) + 1, strdata );
                }
            }

            assert( cnt == unique );
            fwrite( msgidoffset, 1, unique * sizeof( uint64_t ), strmeta );
            delete[] msgidoffset;

            fclose( data );
            fclose( strdata );
            fclose( strmeta );
        }

        delete[] hashdata;
        delete[] distance;
    }

    printf( "\n" );

    // calculate message groups and indirect references
    struct IndirectData
    {
        std::vector<uint32_t> parent;
        std::vector<uint32_t> child;
    };

    std::map<uint32_t, IndirectData> indirect;

    {
        uint8_t tmp[1024];
        HashSearchBig midhash( base + "msgid", base + "midhash.meta", base + "midhash" );

        struct VectorHasher
        {
            size_t operator()( const std::vector<int>& vec ) const
            {
                size_t ret = 0;
                for( auto& i : vec )
                {
                    ret ^= std::hash<uint32_t>()( i );
                }
                return ret;
            }
        };

        std::unordered_map<std::vector<int>, uint32_t, VectorHasher> mapdata;
        std::vector<int> groups;

        uint32_t offset32 = 0;
        FILE* data = fopen( ( base + "midgr" ).c_str(), "wb" );
        FILE* meta = fopen( ( base + "midgr.meta" ).c_str(), "wb" );
        ExpandingBuffer eb;
        for( int i=0; i<unique; i++ )
        {
            if( ( i & 0x3FF ) == 0 )
            {
                printf( "%i/%i\r", i, unique );
                fflush( stdout );
            }

            char decmsg[2048];
            auto declen = compress->Unpack( msgidvec[i], decmsg );

            groups.clear();
            uint32_t hash = XXH32( decmsg, declen - 1, 0 );
            for( int j=0; j<arch.size(); j++ )
            {
                const auto idx = arch[j]->GetMessageIndex( decmsg, hash );
                if( idx != -1 )
                {
                    groups.emplace_back( j );
                }
            }
            assert( !groups.empty() );
            const auto& refarch = arch[groups[0]];
            const auto idx = refarch->GetMessageIndex( decmsg, hash );
            if( refarch->GetParent( idx ) == -1 )
            {
                auto post = refarch->GetMessage( idx, eb );
                auto buf = FindReferences( post );
                if( *buf != '\n' )
                {
                    const auto terminate = buf;
                    int valid = ValidateReferences( buf );
                    if( valid == 0 && buf != terminate )
                    {
                        buf--;
                        for(;;)
                        {
                            while( *buf != '>' && buf != terminate ) buf--;
                            if( buf == terminate ) break;
                            auto end = buf;
                            while( *--buf != '<' ) {}
                            buf++;
                            assert( end - buf < 1024 );
                            ValidateMsgId( buf, end, (char*)tmp );
                            const auto parent = midhash.Search( tmp );
                            if( parent != -1 )
                            {
                                bool ok = true;
                                for( int g=1; g<groups.size(); g++ )
                                {
                                    const auto& currarch = arch[groups[g]];
                                    const auto gmidx = currarch->GetMessageIndex( decmsg );
                                    assert( gmidx != -1 );
                                    const auto pmidx = currarch->GetParent( gmidx );
                                    if( pmidx != -1 )
                                    {
                                        const auto pmid = currarch->GetMessageId( pmidx );
                                        if( strcmp( pmid, (const char*)tmp ) == 0 )
                                        {
                                            ok = false;
                                            break;
                                        }
                                    }
                                }
                                if( ok )
                                {
                                    indirect[i].parent.emplace_back( parent );
                                    indirect[parent].child.emplace_back( i );
                                }
                                break;
                            }
                            if( *buf == '>' ) buf--;
                        }
                    }
                }
            }
            auto it = mapdata.find( groups );
            if( it == mapdata.end() )
            {
                it = mapdata.emplace( groups, offset32 ).first;
                fwrite( &offset32, 1, sizeof( offset32 ), meta );
                uint32_t num = groups.size();
                offset32 += fwrite( &num, 1, sizeof( num ), data );
                offset32 += fwrite( groups.data(), 1, sizeof( uint32_t ) * num, data );
            }
            else
            {
                fwrite( &it->second, 1, sizeof( uint32_t ), meta );
            }
        }
        fclose( data );
        fclose( meta );
    }

    printf( "\nIndirect links: %i\n", indirect.size() );

    {
        struct DenseData
        {
            uint64_t msgid;
            uint32_t parent;
            uint32_t children;
        };
        std::vector<DenseData> dense;
        dense.reserve( indirect.size() );

        uint32_t offset = 0;
        uint32_t zero = 0;
        FILE* data = fopen( ( base + "indirect" ).c_str(), "wb" );
        offset += fwrite( &zero, 1, sizeof( uint32_t ), data );
        for( auto& it : indirect )
        {
            DenseData dd = { it.first };

            const auto& parent = it.second.parent;
            const auto& child = it.second.child;
            if( !parent.empty() )
            {
                dd.parent = offset;
                const uint32_t num = parent.size();
                fwrite( &num, 1, sizeof( uint32_t ), data );
                fwrite( parent.data(), 1, sizeof( uint32_t ) * num, data );
                offset += sizeof( uint32_t ) * ( num + 1 );
            }
            if( !child.empty() )
            {
                dd.children = offset;
                const uint32_t num = child.size();
                fwrite( &num, 1, sizeof( uint32_t ), data );
                fwrite( child.data(), 1, sizeof( uint32_t ) * num, data );
                offset += sizeof( uint32_t ) * ( num + 1 );
            }

            dense.emplace_back( dd );
        }
        fclose( data );

        std::sort( dense.begin(), dense.end(), [] ( const auto& l, const auto& r ) { return l.msgid < r.msgid; } );

        FILE* meta = fopen( ( base + "indirect.dense" ).c_str(), "wb" );
        FILE* off = fopen( ( base + "indirect.offset" ).c_str(), "wb" );
        for( auto& v : dense )
        {
            fwrite( &v.msgid, 1, sizeof( v.msgid ), meta );
            fwrite( &v.parent, 1, sizeof( v.parent ), off );
            fwrite( &v.children, 1, sizeof( v.children ), off );
        }
        fclose( meta );
        fclose( off );
    }

    return 0;
}
