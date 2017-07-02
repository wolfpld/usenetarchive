#include <algorithm>
#include <array>
#include <inttypes.h>
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

    uint64_t cnt = 0;
    uint64_t unique;
    {
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

        FILE* data = fopen( ( base + "msgid" ).c_str(), "wb" );
        FILE* meta = fopen( ( base + "msgid.meta" ).c_str(), "wb" );
        uint64_t offset = 0;
        for( auto& v : msgidset )
        {
            fwrite( &offset, 1, sizeof( offset ), meta );
            offset += fwrite( v, 1, strlen( v ) + 1, data );
        }
        fclose( data );
        fclose( meta );
    }

    MetaView<uint64_t, char> msgid( base + "msgid.meta", base + "msgid" );
    const auto msgidsize = msgid.Size();

    auto hashbits = MsgIdHashBits( msgidsize );
    auto hashsize = MsgIdHashSize( hashbits );
    auto hashmask = MsgIdHashMask( hashbits );
    auto bucket = new std::array<uint32_t, 8>[hashsize];
    auto sizes = std::vector<int>( hashsize );

    cnt = 0;
    for( int i=0; i<msgidsize; i++ )
    {
        if( ( cnt++ & 0x3FFFF ) == 0 )
        {
            printf( "%i/%i\r", cnt, msgidsize );
            fflush( stdout );
        }

        uint32_t hash = XXH32( msgid[i], strlen( msgid[i] ), 0 ) & hashmask;
        if( sizes[hash] == bucket[hash].size() )
        {
            fprintf( stderr, "Too many collisions\n" );
            exit( 1 );
        }
        bucket[hash][sizes[hash]] = i;
        sizes[hash]++;
    }
    printf( "\n" );

    {
        uint64_t zero = 0;
        FILE* data = fopen( ( base + "midhash" ).c_str(), "wb" );
        FILE* meta = fopen( ( base + "midhash.meta" ).c_str(), "wb" );
        uint64_t offset = fwrite( &zero, 1, sizeof( zero ), data );
        cnt = 0;
        for( size_t i=0; i<hashsize; i++ )
        {
            if( ( cnt++ & 0x3FFFF ) == 0 )
            {
                printf( "%i/%i\r", cnt, hashsize );
                fflush( stdout );
            }

            std::sort( bucket[i].begin(), bucket[i].begin() + sizes[i] );

            uint32_t num = sizes[i];
            if( num == 0 )
            {
                fwrite( &zero, 1, sizeof( zero ), meta );
            }
            else
            {
                fwrite( &offset, 1, sizeof( offset ), meta );
                offset += fwrite( &num, 1, sizeof( num ), data );
                offset += fwrite( bucket[i].data(), 1, sizeof( uint32_t ) * num, data );
            }
        }
        fclose( data );
        fclose( meta );
    }

    printf( "\n" );

    struct IndirectData
    {
        std::vector<uint32_t> parent;
        std::vector<uint32_t> child;
    };

    std::map<uint32_t, IndirectData> indirect;

    {
        char tmp[1024];
        HashSearchBig midhash( base + "msgid.meta", base + "msgid", base + "midhash.meta", base + "midhash" );

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
        cnt = 0;
        uint32_t offset32 = 0;
        FILE* data = fopen( ( base + "midgr" ).c_str(), "wb" );
        FILE* meta = fopen( ( base + "midgr.meta" ).c_str(), "wb" );
        ExpandingBuffer eb;
        for( int i=0; i<msgidsize; i++ )
        {
            if( ( cnt++ & 0x3FF ) == 0 )
            {
                printf( "%i/%i\r", cnt, msgidsize );
                fflush( stdout );
            }

            groups.clear();
            auto hash = XXH32( msgid[i], strlen( msgid[i] ), 0 );
            for( int j=0; j<arch.size(); j++ )
            {
                const auto idx = arch[j]->GetMessageIndex( msgid[i], hash );
                if( idx != -1 )
                {
                    groups.emplace_back( j );
                }
            }
            assert( !groups.empty() );
            const auto& refarch = arch[groups[0]];
            const auto idx = refarch->GetMessageIndex( msgid[i], hash );
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
                            ValidateMsgId( buf, end, tmp );
                            const auto parent = midhash.Search( tmp );
                            if( parent != -1 )
                            {
                                bool ok = true;
                                for( int g=1; g<groups.size(); g++ )
                                {
                                    const auto& currarch = arch[groups[g]];
                                    const auto gmidx = currarch->GetMessageIndex( msgid[i] );
                                    assert( gmidx != -1 );
                                    const auto pmidx = currarch->GetParent( gmidx );
                                    if( pmidx != -1 )
                                    {
                                        const auto pmid = currarch->GetMessageId( pmidx );
                                        if( strcmp( pmid, tmp ) == 0 )
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

    printf( "\n" );

    {
        uint32_t offset = 0;
        uint32_t zero = 0;
        cnt = 0;
        FILE* data = fopen( ( base + "indirect" ).c_str(), "wb" );
        FILE* meta = fopen( ( base + "indirect.meta" ).c_str(), "wb" );
        offset += fwrite( &zero, 1, sizeof( uint32_t ), data );
        for( auto& it : indirect )
        {
            while( cnt++ < it.first )
            {
                fwrite( &zero, 1, sizeof( uint32_t ), meta );
                fwrite( &zero, 1, sizeof( uint32_t ), meta );
            }

            const auto& parent = it.second.parent;
            const auto& child = it.second.child;
            if( parent.empty() )
            {
                fwrite( &zero, 1, sizeof( uint32_t ), meta );
            }
            else
            {
                fwrite( &offset, 1, sizeof( uint32_t ), meta );
                const uint32_t num = parent.size();
                fwrite( &num, 1, sizeof( uint32_t ), data );
                fwrite( parent.data(), 1, sizeof( uint32_t ) * num, data );
                offset += sizeof( uint32_t ) * ( num + 1 );
            }
            if( child.empty() )
            {
                fwrite( &zero, 1, sizeof( uint32_t ), meta );
            }
            else
            {
                fwrite( &offset, 1, sizeof( uint32_t ), meta );
                const uint32_t num = child.size();
                fwrite( &num, 1, sizeof( uint32_t ), data );
                fwrite( child.data(), 1, sizeof( uint32_t ) * num, data );
                offset += sizeof( uint32_t ) * ( num + 1 );
            }
        }
        while( cnt++ < msgidsize )
        {
            fwrite( &zero, 1, sizeof( uint32_t ), meta );
            fwrite( &zero, 1, sizeof( uint32_t ), meta );
        }
        fclose( data );
        fclose( meta );
    }

    return 0;
}
