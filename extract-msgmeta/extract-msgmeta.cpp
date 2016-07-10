#include <algorithm>
#include <assert.h>
#include <unordered_map>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "../common/Filesystem.hpp"
#include "../common/MessageView.hpp"
#include "../common/String.hpp"

struct Offsets
{
    uint32_t from;
    uint32_t subject;
};

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s raw\n", argv[0] );
        exit( 1 );
    }

    std::string base = argv[1];
    base.append( "/" );

    MessageView mview( base + "meta", base + "data" );
    const auto size = mview.Size();

    Offsets* data = new Offsets[size];

    FILE* strout = fopen( ( base + "strings" ).c_str(), "wb" );
    uint32_t offset = 0;
    std::unordered_map<std::string, uint32_t> refs;

    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        auto post = mview[i];
        auto buf = post;

        bool fdone = false;
        bool sdone = false;
        const char* fptr;
        const char* sptr;
        while( !fdone || !sdone )
        {
            bool skip = false;
            if( !fdone )
            {
                if( strnicmpl( buf, "from: ", 6 ) == 0 )
                {
                    buf += 6;
                    fptr = buf;
                    skip = true;
                    fdone = true;
                }
            }
            if( !sdone && !skip )
            {
                if( strnicmpl( buf, "subject: ", 9 ) == 0 )
                {
                    buf += 9;
                    sptr = buf;
                    sdone = true;
                }
            }
            while( *buf++ != '\n' ) {};
        }

        auto fend = fptr;
        while( *++fend != '\n' ) {};
        auto send = sptr;
        while( *++send != '\n' ) {};

        std::string fstr( fptr, fend );
        std::string sstr( sptr, send );

        if( refs.find( fstr ) == refs.end() )
        {
            fwrite( fstr.c_str(), 1, fstr.size()+1, strout );
            refs.emplace( fstr, offset );
            offset += fstr.size()+1;
        }
        if( refs.find( sstr ) == refs.end() )
        {
            fwrite( sstr.c_str(), 1, sstr.size()+1, strout );
            refs.emplace( sstr, offset );
            offset += sstr.size()+1;
        }

        data[i].from = refs[fstr];
        data[i].subject = refs[sstr];
    }
    fclose( strout );

    printf( "\nSaving...\n" );
    fflush( stdout );

    FILE* out = fopen( ( base + "strmeta" ).c_str(), "wb" );
    for( uint32_t i=0; i<size; i++ )
    {
        fwrite( &data[i].from, 1, sizeof( Offsets::from ), out );
        fwrite( &data[i].subject, 1, sizeof( Offsets::subject ), out );
    }
    fclose( out );

    delete[] data;

    return 0;
}
