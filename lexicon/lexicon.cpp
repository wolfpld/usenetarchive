#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string>

#ifdef _WIN32
#  include <malloc.h>
#else
#  include <alloca.h>
#endif

#include "../common/MessageView.hpp"
#include "../common/String.hpp"

const char* IgnoredHeaders[] = {
    "newsgroups",
    "date",
    "lines",
    "message-id",
    "mime-version",
    "path",
    "x-trace",
    "x-complaints-to",
    "nntp-posting-date",
    "xref",
    "references",
    "x-in-reply-to",
    "x-newsgroup",
    "x-msmail-priority",
    "x-mimeole",
    "cancel-lock",
    "x-face",
    "x-orig-path",
    "x-tech-contact",
    "x-server-info",
    "x-original-path",
    "x-no-archive",
    "to",
    "supersedes",
    "replyto",
    "reply-to",
    "followup-to",
    "cc",
    nullptr
};

bool IsHeaderIgnored( const char* hdr, const char* end )
{
    int size = end - hdr;
    char* tmp = (char*)alloca( size+1 );
    for( int i=0; i<size; i++ )
    {
        tmp[i] = tolower( hdr[i] );
    }
    tmp[size] = '\0';

    auto test = IgnoredHeaders;
    while( *test )
    {
        if( strncmp( tmp, *test, size+1 ) == 0 )
        {
            return true;
        }
        test++;
    }
    return false;
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

    MessageView mview( base + "meta", base + "data" );
    const auto size = mview.Size();

    for( uint32_t i=0; i<size; i++ )
    {
        if( ( i & 0x1FFF ) == 0 )
        {
            printf( "%i/%i\r", i, size );
            fflush( stdout );
        }

        bool headers = true;

        auto post = mview[i];
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
                while( *end != ':' ) end++;
                end += 2;
                if( IsHeaderIgnored( post, end-2 ) )
                {
                    while( *end != '\n' ) end++;
                }
                else
                {
                    const char* line = end;
                    while( *end != '\n' ) end++;

                }
                post = end + 1;
            }
            else
            {
                break;
            }
        }
    }

    printf( "\n" );

    return 0;
}
