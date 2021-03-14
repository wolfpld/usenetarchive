#include <limits>
#include <memory>
#include <string>
#include <sstream>
#include "../contrib/ini/ini.h"
#include "../contrib/mongoose/mongoose.h"
#ifdef GetMessage
#  undef GetMessage
#endif
#include "../libuat/Archive.hpp"
#include "../libuat/Galaxy.hpp"
#include "../common/MessageLogic.hpp"

static void TryIni( const char*& value, ini_t* config, const char* section, const char* key )
{
    auto tmp = ini_get( config, section, key );
    if( tmp ) value = tmp;
}

static const std::string IntroPage( R"WEB(
<!doctype html>
<html>
<head>
<style>
body { position: absolute; top: 50%; left:50%; transform: translate(-50%, -50%); text-align: center; }
</style>
<meta charset="utf-8">
<title>Usenet Archive Message-ID search</title>
</head>
<body>
<form method="post">
<input type="text" size=60 name="msgid" placeholder="Message-ID"/><br><br>
<input type="submit" value="View">
</form>
</body>
</html>
)WEB" );

static ExpandingBuffer eb;
static std::unique_ptr<Galaxy> galaxy;
static int chomp;

static void Handler( struct mg_connection* nc, int ev, void* data )
{
    if( ev != MG_EV_HTTP_REQUEST ) return;
    auto hm = (struct http_message*)data;
    char remoteAddr[100];
    mg_sock_to_str( nc->sock, remoteAddr, sizeof(remoteAddr), MG_SOCK_STRINGIFY_REMOTE | MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT );
    std::string uri( hm->uri.p, hm->uri.len );
    auto ua = mg_get_http_header( hm, "User-Agent" );
    int code = 0, size = 0;
    if( uri.size() <= chomp )
    {
        code = 500;
        mg_http_send_error( nc, code, nullptr );
    }
    else if( strcmp( uri.c_str() + chomp, "/" ) == 0 )
    {
        if( hm->body.len == 0 )
        {
            code = 200;
            size = IntroPage.size();
            mg_send_head( nc, 200, size, "Content-Type: text/html" );
            mg_printf( nc, "%.*s", size, IntroPage.c_str() );
        }
        else
        {
            char msgid[4096];
            if( mg_get_http_var( &hm->body, "msgid", msgid, 4096 ) > 0 )
            {
                code = 301;
                size = hm->body.len;
                mg_http_send_redirect( nc, code, mg_mk_str( msgid ), mg_mk_str( "Cache-Control: no-cache, no-store, must-revalidate" ) );
            }
            else
            {
                code = 400;
                mg_http_send_error( nc, code, nullptr );
            }
        }
    }
    else
    {
        bool found = false;
        if( IsMsgId( uri.c_str() + chomp + 1, uri.c_str() + uri.size() ) )
        {
            uint8_t packed[4096];
            galaxy->PackMsgId( uri.c_str() + chomp + 1, packed );
            auto idx = galaxy->GetMessageIndex( packed );
            if( idx >= 0 )
            {
                auto groups = galaxy->GetGroups( idx );
                for( uint64_t i=0; i<groups.size; i++ )
                {
                    if( galaxy->IsArchiveAvailable( groups.ptr[i] ) )
                    {
                        auto& archive = *galaxy->GetArchive( groups.ptr[i] );
                        uint8_t archivePacked[4096];
                        archive.RepackMsgId( packed, archivePacked, galaxy->GetCompress() );
                        const auto message = archive.GetMessage( archivePacked, eb );

                        code = 200;
                        size = strlen( message );
                        mg_send_head( nc, 200, size, "Content-Type: text/plain; charset=utf-8" );
                        mg_printf( nc, "%.*s", size, message );

                        found = true;
                        break;
                    }
                }
            }
        }

        if( !found )
        {
            code = 404;
            mg_http_send_error( nc, 404, nullptr );
        }
    }
    printf( "%s \"%.*s %.*s\" %i %i \"%.*s\"\n", remoteAddr, (int)hm->method.len, hm->method.p, (int)hm->uri.len, hm->uri.p, code, size, ua ? (int)ua->len : 0, ua ? ua->p : "" );
    fflush( stdout );
}

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "Usage: %s /path/to/config.ini\n", argv[0] );
        fflush( stderr );
        return 1;
    }

    auto config = ini_load( argv[1] );
    if( !config )
    {
        fprintf( stderr, "Cannot open config file %s!\n", argv[1] );
        fflush( stderr );
        return 2;
    }

    const char* bind = "127.0.0.1";
    const char* port = "8119";
    const char* galaxyPath = "news/galaxy";
    const char* chompStr = "0";

    TryIni( bind, config, "server", "bind" );
    TryIni( port, config, "server", "port" );
    TryIni( chompStr, config, "server", "chomp" );
    TryIni( galaxyPath, config, "galaxy", "path" );

    chomp = atoi( chompStr );

    galaxy.reset( Galaxy::Open( galaxyPath ) );
    if( !galaxy )
    {
        fprintf( stderr, "Cannot access galaxy at %s!\n", galaxyPath );
        fflush( stderr );
        ini_free( config );
        return 3;
    }

    char address[1024];
    snprintf( address, 1024, "%s:%s", bind, port );

    struct mg_mgr mgr;
    mg_mgr_init( &mgr, nullptr );
    auto conn = mg_bind( &mgr, address, Handler );
    if( !conn )
    {
        fprintf( stderr, "Cannot bind to %s!\n", address );
        fflush( stderr );
        ini_free( config );
        return 4;
    }
    mg_set_protocol_http_websocket( conn );

    printf( "Listening on %s...\n", address );
    fflush( stdout );
    for(;;)
    {
        mg_mgr_poll( &mgr, std::numeric_limits<int>::max() );
    }

    ini_free( config );
    return 0;
}
