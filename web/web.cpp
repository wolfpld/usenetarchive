#include <limits>
#include <memory>
#include <string>
#include "../contrib/ini/ini.h"
#include "../contrib/mongoose/mongoose.h"
#ifdef GetMessage
#  undef GetMessage
#endif
#include "../libuat/Archive.hpp"
#include "../libuat/Galaxy.hpp"
#include "../common/KillRe.hpp"
#include "../common/MessageLines.hpp"
#include "../common/MessageLogic.hpp"
#include "../common/UTF8.hpp"

static void TryIni( const char*& value, ini_t* config, const char* section, const char* key )
{
    auto tmp = ini_get( config, section, key );
    if( tmp ) value = tmp;
}

static const std::string IntroPage( R"WEB(<!doctype html>
<html>
<head>
<style>
body { position: absolute; top: 50%; left:50%; transform: translate(-50%, -50%); text-align: center; background-color: #111111; }
div { background-color: #222222; padding: 2em; }
</style>
<meta charset="utf-8">
<title>Usenet Archive Message-ID search</title>
</head>
<body>
<div>
<form method="post">
<input type="text" size=60 name="msgid" placeholder="Message-ID"/><br><br>
<input type="submit" value="View">
</form>
</div>
</body>
</html>
)WEB" );

static const std::string MessageHeader( R"WEB(<!doctype html>
<html>
<head>
<link rel="stylesheet" href="https://fonts.googleapis.com/css?family=Roboto+Mono"/>
<style>
body { background-color: #111111; }
.message { font-family: 'Roboto Mono', Lucida Console, Courier, monospace; font-size: 15px; white-space: pre-wrap; width: 100%; min-width: 640px; max-width: 1024px; margin-left: auto; margin-right: auto; background-color: #222222; padding: 1em; color: #cccccc }
.hdrName { color: #13a10e }
.hdrBody { color: #3a96dd }
.q1 { color: #c50f1f }
.q2 { color: #881798 }
.q3 { color: #3b78ff }
.q4 { color: #13a10e }
.q5 { color: #c19c00 }
.signature { color: #767676 }
</style>
<meta charset="utf-8">
<title>
)WEB" );

static ExpandingBuffer eb;
static MessageLines ml;
static KillRe killre;
static std::unique_ptr<Galaxy> galaxy;
static int chomp;
static std::string tmpStr;

static std::string Encode( const char* txt, const char* end )
{
    std::string enc;
    while( txt < end )
    {
        if( *txt == '&' )
        {
            enc += "&amp;";
            txt++;
        }
        else if( *txt == '<' )
        {
            enc += "&lt;";
            txt++;
        }
        else if( *txt == '>' )
        {
            enc += "&gt;";
            txt++;
        }
        else
        {
            const auto len = codepointlen( *txt );
            enc.append( txt, txt+len );
            txt += len;
        }
    }
    return enc;
}

static std::string Encode( const char* txt )
{
    return Encode( txt, txt + strlen( txt ) );
}

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
                        const auto idx = archive.GetMessageIndex( archivePacked );
                        const auto message = archive.GetMessage( idx, eb );
                        ml.PrepareLines( message, false );

                        tmpStr = MessageHeader;
                        tmpStr += Encode( archive.GetRealName( idx ) ) + ", " + Encode( killre.Kill( archive.GetSubject( idx ) ) ) + "</title></head><body>\n<div class=\"message\">";

                        auto& lines = ml.Lines();
                        auto& parts = ml.Parts();
                        for( auto& line : lines )
                        {
                            for( int i=0; i<line.parts; i++ )
                            {
                                auto& part = parts[line.idx+i];

                                bool noSpan = false;
                                if( part.flags == MessageLines::L_HeaderName ) tmpStr += "<span class=\"hdrName\">";
                                else if( part.flags == MessageLines::L_HeaderBody ) tmpStr += "<span class=\"hdrBody\">";
                                else if( part.flags == MessageLines::L_Quote0 ) noSpan = true;
                                else if( part.flags == MessageLines::L_Quote1 ) tmpStr += "<span class=\"q1\">";
                                else if( part.flags == MessageLines::L_Quote2 ) tmpStr += "<span class=\"q2\">";
                                else if( part.flags == MessageLines::L_Quote3 ) tmpStr += "<span class=\"q3\">";
                                else if( part.flags == MessageLines::L_Quote4 ) tmpStr += "<span class=\"q4\">";
                                else if( part.flags == MessageLines::L_Quote5 ) tmpStr += "<span class=\"q5\">";
                                else if( part.flags == MessageLines::L_Signature ) tmpStr += "<span class=\"signature\">";
                                else noSpan = true;

                                const bool du = part.deco == MessageLines::D_Underline;
                                const bool di = part.deco == MessageLines::D_Italics;
                                const bool db = part.deco == MessageLines::D_Bold;
                                if( du ) tmpStr += "<u>";
                                else if( di ) tmpStr += "<i>";
                                else if( di ) tmpStr += "<em>";

                                tmpStr += Encode( message + part.offset, message + part.offset + part.len );

                                if( du ) tmpStr += "</u>";
                                else if( di ) tmpStr += "</i>";
                                else if( di ) tmpStr += "</em>";

                                if( !noSpan ) tmpStr += "</span>";
                            }
                            tmpStr += "\n";
                        }

                        tmpStr += "</div></body></html>";

                        code = 200;
                        size = tmpStr.size();
                        mg_send_head( nc, 200, size, "Content-Type: text/html" );
                        mg_printf( nc, "%.*s", size, tmpStr.c_str() );

                        ml.Reset();
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
