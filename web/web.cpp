#include <limits>
#include <memory>
#include <string>
#include <time.h>

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
<title>Usenet Archive Message-ID search</title>
</head>
<body>
<div>
<form method="post">
<input type="text" size=60 name="msgid" placeholder="Message-ID"/><br><br>
<input type="submit" value="View">
</form>
</div>
)WEB" );

static const std::string IntroFooter( "</body></html>" );

static const std::string MessageHeader( R"WEB(<!doctype html>
<html>
<head>
<script language="javascript" type="text/javascript">
function toggleHide()
{
    var e = document.getElementsByClassName( "hide" );
    for( var i=0; i<e.length; i++ )
    {
        if( e[i].style.display == "block" ) e[i].style.display = "none";
        else e[i].style.display = "block";
    }
}
</script>
<link rel="stylesheet" href="https://fonts.googleapis.com/css?family=Roboto+Mono"/>
<style>
body { background-color: #111111; }
a:link { text-decoration-style: dotted; color: inherit }
a:hover { text-decoration-style: solid }
a:visited { color: inherit }
.message { font-family: 'Roboto Mono', Lucida Console, Courier, monospace; font-size: 15px; white-space: pre-wrap; min-width: 680px; max-width: 1024px; margin-left: auto; margin-right: auto; background-color: #222222; padding: 1em; color: #cccccc }
.hdrName { color: #13a10e }
.hdrBody { color: #3a96dd }
.q1 { color: #c50f1f }
.q2 { color: #881798 }
.q3 { color: #3b78ff }
.q4 { color: #13a10e }
.q5 { color: #c19c00 }
.signature { color: #767676 }
.hide { display: none }
</style>
<title>)WEB" );

static ExpandingBuffer eb;
static MessageLines ml;
static KillRe killre;
static std::unique_ptr<Galaxy> galaxy;
static int chomp;
static std::string tmpStr;
static const char* tracker = "";
static size_t trackerLen;

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
        else if( *txt == '"' )
        {
            enc += "&quot;";
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

static std::string UriEncode( const char* txt, size_t sz )
{
    std::string str;
    str.reserve( sz );
    const auto end = txt + sz;
    while( txt < end )
    {
        if( *txt == '%' )
        {
            str += "%25";
        }
        else
        {
            str.push_back( *txt );
        }
        txt++;
    }
    return str;
}

static std::string UriDecode( const char* txt, size_t sz )
{
    std::string str;
    str.reserve( sz );
    const auto end = txt + sz;
    while( txt < end )
    {
        str.push_back( *txt );
        if( *txt == '%' )
        {
            txt += 3;
        }
        else
        {
            txt++;
        }
    }
    return str;
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
            size = IntroPage.size() + IntroFooter.size() + trackerLen;
            mg_send_head( nc, 200, size, "Content-Type: text/html; charset=utf-8" );
            mg_printf( nc, "%s%s%s", IntroPage.c_str(), tracker, IntroFooter.c_str() );
        }
        else
        {
            char msgid[4096];
            if( mg_get_http_var( &hm->body, "msgid", msgid, 4096 ) > 0 )
            {
                const auto encoded = UriEncode( msgid, hm->body.len );
                code = 301;
                size = encoded.size();
                mg_http_send_redirect( nc, code, mg_mk_str( encoded.c_str() ), mg_mk_str( "Cache-Control: no-cache, no-store, must-revalidate" ) );
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
        const auto decoded = UriDecode( uri.c_str() + chomp + 1, uri.size() - chomp - 1 );
        if( IsMsgId( decoded.c_str(), decoded.c_str() + decoded.size() ) )
        {
            uint8_t packed[4096];
            galaxy->PackMsgId( decoded.c_str(), packed );
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

                        const auto groupName = archive.GetArchiveName();
                        time_t t = { archive.GetDate( idx ) };
                        char dateStr[64];
                        strftime( dateStr, 64, "%Y-%m-%d %H:%M:%S %z", localtime( &t ) );
                        const auto desc = Encode( groupName.first, groupName.first + groupName.second ) + ", " + dateStr;
                        const auto title = Encode( archive.GetRealName( idx ) ) + ", " + Encode( killre.Kill( archive.GetSubject( idx ) ) );

                        tmpStr = MessageHeader;
                        tmpStr += title + "</title><meta property=\"og:title\" content=\"" + title + "\"/><meta property=\"og:type\" content=\"website\"/><meta property=\"og:site_name\" content=\"Usenet Archive\"/><meta property=\"og:description\" content=\"" + desc + "\"/></head><body>\n<div class=\"message\">";

                        auto& lines = ml.Lines();
                        auto& parts = ml.Parts();
                        for( auto& line : lines )
                        {
                            const bool isHeader = line.parts > 0 && parts[line.idx].flags == MessageLines::L_HeaderName;
                            if( isHeader )
                            {
                                tmpStr += "<span";
                                if( !line.essential ) tmpStr += " class=\"hide\"";
                                tmpStr += " onclick=\"toggleHide()\">";
                            }
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
                                const bool dl = part.deco == MessageLines::D_Url;
                                if( du ) tmpStr += "<u>";
                                else if( di ) tmpStr += "<i>";
                                else if( db ) tmpStr += "<b>";
                                else if( dl )
                                {
                                    tmpStr += "<a href=\"";
                                    if( part.len > 5 && memcmp( message + part.offset, "news:", 5 ) == 0 )
                                    {
                                        tmpStr += Encode( message + part.offset, message + part.offset + part.len ).c_str() + 5;
                                    }
                                    else
                                    {
                                        tmpStr += Encode( message + part.offset, message + part.offset + part.len );
                                    }
                                    tmpStr += "\">";
                                }

                                tmpStr += Encode( message + part.offset, message + part.offset + part.len );

                                if( du ) tmpStr += "</u>";
                                else if( di ) tmpStr += "</i>";
                                else if( db ) tmpStr += "</b>";
                                else if( dl ) tmpStr += "</a>";

                                if( !noSpan ) tmpStr += "</span>";
                            }
                            tmpStr += "\n";
                            if( isHeader ) tmpStr += "</span>";
                        }

                        tmpStr += "</div>";
                        tmpStr += tracker;
                        tmpStr += "</body></html>";

                        code = 200;
                        size = tmpStr.size();
                        mg_send_head( nc, 200, size, "Content-Type: text/html; charset=utf-8" );
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
    TryIni( tracker, config, "server", "tracker" );
    TryIni( galaxyPath, config, "galaxy", "path" );

    chomp = atoi( chompStr );
    trackerLen = strlen( tracker );

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
