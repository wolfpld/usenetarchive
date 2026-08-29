#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <time.h>
#include <vector>

#include "../contrib/ini/ini.h"
#include "../contrib/mongoose/mongoose.h"
#ifdef GetMessage
#  undef GetMessage
#endif
#include "../libuat/Archive.hpp"
#include "../libuat/Galaxy.hpp"
#include "../libuat/SearchEngine.hpp"
#include "../common/KillRe.hpp"
#include "../common/MessageLines.hpp"
#include "../common/MessageLogic.hpp"
#include "../common/UTF8.hpp"

static void TryIni( const char*& value, ini_t* config, const char* section, const char* key )
{
    auto tmp = ini_get( config, section, key );
    if( tmp ) value = tmp;
}

static ExpandingBuffer eb;
static MessageLines ml;
static KillRe killre;
static std::unique_ptr<Galaxy> galaxy;
static std::map<std::string, int> groupIndex;

static const size_t ThreadListPageSize = 50;
static const size_t SearchGlobalCap = 200;

struct ResultRow
{
    std::string group;
    uint32_t idx;
    float rank;
};

static const char* CSS = R"CSS(
body { background:#111111; color:#cccccc; font-family: 'Roboto Mono', Menlo, Consolas, monospace; margin:0; padding:0; }
a { color:#3a96dd; text-decoration:none; }
a:hover { text-decoration:underline; }
header { background:#1b1b1b; padding:0.8em 1.5em; border-bottom:1px solid #333333; display:flex; justify-content:space-between; align-items:center; flex-wrap:wrap; gap:0.5em; }
header a.brand { color:#ffffff; font-weight:bold; font-size:1.1em; }
form.search input[type=text] { background:#222222; border:1px solid #444444; color:#cccccc; padding:0.3em 0.6em; font-family: inherit; }
form.search input[type=submit] { background:#2a2a2a; border:1px solid #444444; color:#cccccc; padding:0.3em 0.8em; cursor:pointer; font-family: inherit; }
main { max-width:1024px; margin: 1.5em auto; padding: 0 1em 3em 1em; }
h1 { font-size:1.4em; }
table { width:100%; border-collapse:collapse; }
th, td { text-align:left; padding:0.4em 0.6em; border-bottom:1px solid #222222; vertical-align:top; }
th { color:#888888; font-weight:normal; font-size:0.8em; text-transform:uppercase; }
tr:hover { background:#181818; }
.count { color:#888888; font-size:0.85em; }
.pager { margin-top:1em; display:flex; gap:1.5em; align-items:center; }
.thread-msg { background:#1a1a1a; border:1px solid #262626; border-radius:4px; margin: 0.6em 0; padding:0.8em; }
.msg-hdr { color:#888888; font-size:0.85em; margin-bottom:0.5em; }
.msg-hdr .from { color:#13a10e; }
.msg-hdr .subj { color:#3a96dd; }
.msg-body { white-space:pre-wrap; font-size:0.92em; }
.hdrName { color:#13a10e }
.hdrBody { color:#3a96dd }
.q1 { color:#c50f1f }
.q2 { color:#881798 }
.q3 { color:#3b78ff }
.q4 { color:#13a10e }
.q5 { color:#c19c00 }
.signature { color:#767676 }
.hide { display:none }
.rank { color:#666666; font-size:0.8em; }
)CSS";

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
    if( !txt ) return std::string();
    return Encode( txt, txt + strlen( txt ) );
}

static std::string Encode( const std::string& txt )
{
    return Encode( txt.c_str(), txt.c_str() + txt.size() );
}

static int HexVal( char c )
{
    if( c >= '0' && c <= '9' ) return c - '0';
    c = (char)tolower( (unsigned char)c );
    if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
    return -1;
}

static std::string UriDecode( const char* txt, size_t sz )
{
    std::string str;
    str.reserve( sz );
    size_t i = 0;
    while( i < sz )
    {
        char c = txt[i];
        if( c == '%' && i+2 < sz )
        {
            int h1 = HexVal( txt[i+1] );
            int h2 = HexVal( txt[i+2] );
            if( h1 >= 0 && h2 >= 0 )
            {
                str.push_back( (char)( ( h1 << 4 ) | h2 ) );
                i += 3;
                continue;
            }
        }
        str.push_back( c );
        i++;
    }
    return str;
}

static std::string UriPathEncode( const std::string& s )
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    out.reserve( s.size() * 3 );
    for( unsigned char c : s )
    {
        if( isalnum( c ) || c == '-' || c == '_' || c == '.' || c == '~' )
        {
            out.push_back( (char)c );
        }
        else
        {
            out.push_back( '%' );
            out.push_back( hex[c >> 4] );
            out.push_back( hex[c & 0xF] );
        }
    }
    return out;
}

static std::vector<std::string> SplitPath( const std::string& uri )
{
    std::vector<std::string> parts;
    size_t i = uri.empty() ? 0 : 1;
    while( i < uri.size() )
    {
        auto j = uri.find( '/', i );
        if( j == std::string::npos ) j = uri.size();
        if( j > i ) parts.emplace_back( UriDecode( uri.c_str() + i, j - i ) );
        i = j + 1;
    }
    return parts;
}

static std::string GetQueryVar( struct mg_str qs, const char* name )
{
    // MG_MAX_HTTP_REQUEST_SIZE bounds the entire request line, so the decoded
    // value of any query variable can never exceed it. A smaller buffer here
    // would let mg_get_http_var silently drop long-but-valid values (returns
    // -3) instead of reading them.
    char buf[MG_MAX_HTTP_REQUEST_SIZE];
    int n = mg_get_http_var( &qs, name, buf, sizeof( buf ) );
    if( n > 0 ) return std::string( buf, n );
    return std::string();
}

static std::string FormatDate( uint32_t unixTime )
{
    time_t t = (time_t)unixTime;
    char buf[64];
    strftime( buf, sizeof( buf ), "%Y-%m-%d %H:%M", localtime( &t ) );
    return buf;
}

static std::string FormatRank( float r )
{
    char buf[32];
    snprintf( buf, sizeof( buf ), "%.2f", r );
    return buf;
}

static std::string PageHeader( const std::string& title )
{
    std::string s = "<!doctype html>\n<html>\n<head>\n<meta charset=\"utf-8\"/>\n";
    s += "<link rel=\"stylesheet\" href=\"https://fonts.googleapis.com/css?family=Roboto+Mono\"/>\n";
    s += "<script language=\"javascript\" type=\"text/javascript\">\nfunction toggleHide(){var e=document.getElementsByClassName(\"hide\");for(var i=0;i<e.length;i++){if(e[i].style.display==\"block\")e[i].style.display=\"none\";else e[i].style.display=\"block\";}}\n</script>\n";
    s += "<title>";
    s += Encode( title.c_str() );
    s += " - Usenet Archive</title>\n<style>";
    s += CSS;
    s += "</style>\n</head>\n<body>\n";
    s += "<header><a class=\"brand\" href=\"/\">Usenet Archive</a>";
    s += "<form class=\"search\" action=\"/search\" method=\"get\"><input type=\"text\" name=\"q\" placeholder=\"Search all groups\"/><input type=\"submit\" value=\"Search\"/></form>";
    s += "</header>\n<main>\n";
    return s;
}

static std::string PageFooter()
{
    return "\n</main>\n</body>\n</html>\n";
}

static bool IsSafeScheme( const char* txt, size_t len )
{
    static const char* allowed[] = { "http://", "https://", "ftp://", "mailto:", "news:" };
    for( auto scheme : allowed )
    {
        size_t sl = strlen( scheme );
        if( len >= sl && memcmp( txt, scheme, sl ) == 0 ) return true;
    }
    return false;
}

static std::string RenderMessage( const char* message )
{
    ml.PrepareLines( message, false );
    std::string out;
    auto& lines = ml.Lines();
    auto& parts = ml.Parts();
    for( auto& line : lines )
    {
        const bool isHeader = line.parts > 0 && parts[line.idx].flags == MessageLines::L_HeaderName;
        if( isHeader )
        {
            out += "<span";
            if( !line.essential ) out += " class=\"hide\"";
            out += " onclick=\"toggleHide()\">";
        }
        for( int i=0; i<line.parts; i++ )
        {
            auto& part = parts[line.idx+i];

            bool noSpan = false;
            if( part.flags == MessageLines::L_HeaderName ) out += "<span class=\"hdrName\">";
            else if( part.flags == MessageLines::L_HeaderBody ) out += "<span class=\"hdrBody\">";
            else if( part.flags == MessageLines::L_Quote0 ) noSpan = true;
            else if( part.flags == MessageLines::L_Quote1 ) out += "<span class=\"q1\">";
            else if( part.flags == MessageLines::L_Quote2 ) out += "<span class=\"q2\">";
            else if( part.flags == MessageLines::L_Quote3 ) out += "<span class=\"q3\">";
            else if( part.flags == MessageLines::L_Quote4 ) out += "<span class=\"q4\">";
            else if( part.flags == MessageLines::L_Quote5 ) out += "<span class=\"q5\">";
            else if( part.flags == MessageLines::L_Signature ) out += "<span class=\"signature\">";
            else noSpan = true;

            const bool du = part.deco == MessageLines::D_Underline;
            const bool di = part.deco == MessageLines::D_Italics;
            const bool db = part.deco == MessageLines::D_Bold;
            bool dlSafe = false;
            const bool dl = part.deco == MessageLines::D_Url;
            if( du ) out += "<u>";
            else if( di ) out += "<i>";
            else if( db ) out += "<b>";
            else if( dl )
            {
                if( part.len > 5 && memcmp( message + part.offset, "news:", 5 ) == 0 )
                {
                    std::string mid( message + part.offset + 5, message + part.offset + part.len );
                    out += "<a href=\"/msgid/" + UriPathEncode( mid ) + "\">";
                    dlSafe = true;
                }
                else if( IsSafeScheme( message + part.offset, part.len ) )
                {
                    out += "<a href=\"" + Encode( message + part.offset, message + part.offset + part.len ) + "\">";
                    dlSafe = true;
                }
            }

            out += Encode( message + part.offset, message + part.offset + part.len );

            if( du ) out += "</u>";
            else if( di ) out += "</i>";
            else if( db ) out += "</b>";
            else if( dlSafe ) out += "</a>";

            if( !noSpan ) out += "</span>";
        }
        out += "\n";
        if( isHeader ) out += "</span>";
    }
    ml.Reset();
    return out;
}

static std::string RenderGroupList()
{
    std::string body = "<h1>Newsgroups</h1>";
    auto avail = galaxy->GetAvailableArchives();
    std::sort( avail.begin(), avail.end(), []( int a, int b ) {
        return strcmp( galaxy->GetArchiveName( a ), galaxy->GetArchiveName( b ) ) < 0;
    } );
    body += "<p class=\"count\">" + std::to_string( avail.size() ) + " groups</p>";
    body += "<table><thead><tr><th>Name</th><th>Description</th><th>Messages</th><th>Threads</th></tr></thead><tbody>";
    for( int idx : avail )
    {
        const char* name = galaxy->GetArchiveName( idx );
        const char* desc = galaxy->GetArchiveDescription( idx );
        body += "<tr><td><a href=\"/group/" + Encode( name ) + "\">" + Encode( name ) + "</a></td><td>" + Encode( desc ) + "</td><td>" +
            std::to_string( galaxy->NumberOfMessages( idx ) ) + "</td><td>" + std::to_string( galaxy->NumberOfTopLevel( idx ) ) + "</td></tr>";
    }
    body += "</tbody></table>";
    return body;
}

static std::string RenderThreadList( const std::string& name, int archiveIdx, size_t page )
{
    auto& archive = *galaxy->GetArchive( archiveIdx, false );
    auto topLevel = archive.GetTopLevel();
    size_t total = topLevel.size;
    size_t totalPages = ( total + ThreadListPageSize - 1 ) / ThreadListPageSize;
    if( totalPages == 0 ) totalPages = 1;
    if( page >= totalPages ) page = totalPages - 1;
    size_t start = page * ThreadListPageSize;
    size_t end = std::min( total, start + ThreadListPageSize );

    std::string body = "<h1>" + Encode( name ) + "</h1>";
    auto desc = archive.GetShortDescription();
    if( desc.second ) body += "<p class=\"count\">" + Encode( desc.first, desc.first + desc.second ) + "</p>";
    body += "<p class=\"count\">" + std::to_string( archive.NumberOfMessages() ) + " messages, " + std::to_string( total ) + " threads</p>";
    body += "<form class=\"search\" action=\"/group/" + Encode( name ) + "/search\" method=\"get\"><input type=\"text\" name=\"q\" placeholder=\"Search this group\"/><input type=\"submit\" value=\"Search\"/></form>";

    body += "<table><thead><tr><th>Subject</th><th>From</th><th>Date</th><th>Replies</th></tr></thead><tbody>";
    for( size_t i=start; i<end; i++ )
    {
        uint32_t idx = topLevel.ptr[i];
        const char* subj = archive.GetSubject( idx );
        const char* from = archive.GetRealName( idx );
        uint32_t date = archive.GetDate( idx );
        uint32_t cnt = archive.GetTotalChildrenCount( idx );
        uint32_t replies = cnt > 0 ? cnt - 1 : 0;
        body += "<tr><td><a href=\"/group/" + Encode( name ) + "/thread/" + std::to_string( idx ) + "\">" + Encode( killre.Kill( subj ) ) + "</a></td><td>" +
            Encode( from ) + "</td><td>" + FormatDate( date ) + "</td><td>" + std::to_string( replies ) + "</td></tr>";
    }
    body += "</tbody></table>";

    body += "<div class=\"pager\">";
    if( page > 0 ) body += "<a href=\"/group/" + Encode( name ) + "?page=" + std::to_string( page-1 ) + "\">&laquo; Prev</a>";
    body += "<span class=\"count\">Page " + std::to_string( page+1 ) + " of " + std::to_string( totalPages ) + "</span>";
    if( page+1 < totalPages ) body += "<a href=\"/group/" + Encode( name ) + "?page=" + std::to_string( page+1 ) + "\">Next &raquo;</a>";
    body += "</div>";

    return body;
}

static void RenderThreadRec( Archive& archive, const std::string& name, uint32_t idx, int depth, std::string& body )
{
    const char* msg = archive.GetMessage( idx, eb );
    if( msg )
    {
        const char* subj = archive.GetSubject( idx );
        const char* from = archive.GetRealName( idx );
        uint32_t date = archive.GetDate( idx );
        body += "<div class=\"thread-msg\" style=\"margin-left:" + std::to_string( depth * 24 ) + "px\">";
        body += "<div class=\"msg-hdr\"><span class=\"from\">" + Encode( from ) + "</span> &mdash; <span class=\"subj\">" +
            Encode( killre.Kill( subj ) ) + "</span> &mdash; " + FormatDate( date ) + " &mdash; <a href=\"/group/" + Encode( name ) +
            "/msg/" + std::to_string( idx ) + "\">#</a></div>";
        body += "<div class=\"msg-body\">" + RenderMessage( msg ) + "</div>";
        body += "</div>";
    }
    auto children = archive.GetChildren( idx );
    for( uint64_t i=0; i<children.size; i++ )
    {
        RenderThreadRec( archive, name, children.ptr[i], depth+1, body );
    }
}

static uint32_t FindRoot( Archive& archive, uint32_t idx )
{
    uint32_t steps = 0;
    const uint32_t maxSteps = archive.NumberOfMessages() + 1;
    for(;;)
    {
        int32_t p = archive.GetParent( idx );
        if( p < 0 || ++steps > maxSteps ) return idx;
        idx = (uint32_t)p;
    }
}

static std::string RenderThread( const std::string& name, int archiveIdx, uint32_t anchorIdx )
{
    auto& archive = *galaxy->GetArchive( archiveIdx, false );
    uint32_t root = FindRoot( archive, anchorIdx );
    std::string body = "<p><a href=\"/group/" + Encode( name ) + "\">&laquo; " + Encode( name ) + "</a></p>";
    RenderThreadRec( archive, name, root, 0, body );
    return body;
}

static std::string RenderSingleMessage( const std::string& name, int archiveIdx, uint32_t idx )
{
    auto& archive = *galaxy->GetArchive( archiveIdx, false );
    const char* msg = archive.GetMessage( idx, eb );
    if( !msg ) return std::string();

    uint32_t root = FindRoot( archive, idx );
    std::string body = "<p><a href=\"/group/" + Encode( name ) + "\">&laquo; " + Encode( name ) + "</a> &middot; <a href=\"/group/" +
        Encode( name ) + "/thread/" + std::to_string( root ) + "\">View full thread</a></p>";

    const char* subj = archive.GetSubject( idx );
    const char* from = archive.GetRealName( idx );
    uint32_t date = archive.GetDate( idx );
    body += "<div class=\"thread-msg\">";
    body += "<div class=\"msg-hdr\"><span class=\"from\">" + Encode( from ) + "</span> &mdash; <span class=\"subj\">" +
        Encode( killre.Kill( subj ) ) + "</span> &mdash; " + FormatDate( date ) + "</div>";
    body += "<div class=\"msg-body\">" + RenderMessage( msg ) + "</div>";
    body += "</div>";
    return body;
}

static std::vector<ResultRow> SearchArchive( Archive& archive, const std::string& groupName, const std::string& query, size_t cap )
{
    std::vector<ResultRow> out;
    if( query.empty() ) return out;
    SearchEngine se( archive );
    auto data = se.Search( query.c_str(), SearchEngine::SF_AdjacentWords );
    auto& results = data.results;
    if( results.empty() ) return out;
    std::sort( results.begin(), results.end(), []( const SearchResult& a, const SearchResult& b ) { return a.rank > b.rank; } );
    size_t n = std::min( results.size(), cap );
    out.reserve( n );
    // Normalize ranks to [0,1] relative to this archive's own top match. Raw
    // TF/IDF-style scores aren't comparable across corpora of different sizes;
    // this at least puts every group's contribution to a merged result set on
    // a common scale instead of comparing raw magnitudes directly.
    const float maxRank = results[0].rank;
    const float norm = maxRank > 0.f ? 1.f / maxRank : 1.f;
    for( size_t i=0; i<n; i++ ) out.push_back( { groupName, results[i].postid, results[i].rank * norm } );
    return out;
}

static std::string RenderResults( const std::vector<ResultRow>& rows, const std::string& query, const std::string& action, bool truncated )
{
    std::string body = "<h1>Search results</h1>";
    body += "<form class=\"search\" action=\"" + action + "\" method=\"get\"><input type=\"text\" name=\"q\" value=\"" +
        Encode( query.c_str() ) + "\"/><input type=\"submit\" value=\"Search\"/></form>";
    if( query.empty() ) return body;
    if( rows.empty() )
    {
        body += "<p class=\"count\">No results.</p>";
        return body;
    }
    body += "<p class=\"count\">" + std::to_string( rows.size() ) + ( truncated ? "+" : "" ) + " results</p>";
    body += "<table><thead><tr><th>Group</th><th>Subject</th><th>From</th><th>Date</th><th>Rank</th></tr></thead><tbody>";
    for( auto& r : rows )
    {
        auto it = groupIndex.find( r.group );
        if( it == groupIndex.end() ) continue;
        auto& archive = *galaxy->GetArchive( it->second, false );
        const char* subj = archive.GetSubject( r.idx );
        const char* from = archive.GetRealName( r.idx );
        uint32_t date = archive.GetDate( r.idx );
        body += "<tr><td><a href=\"/group/" + Encode( r.group ) + "\">" + Encode( r.group ) + "</a></td><td><a href=\"/group/" +
            Encode( r.group ) + "/msg/" + std::to_string( r.idx ) + "\">" + Encode( killre.Kill( subj ) ) + "</a></td><td>" +
            Encode( from ) + "</td><td>" + FormatDate( date ) + "</td><td class=\"rank\">" + FormatRank( r.rank ) + "</td></tr>";
    }
    body += "</tbody></table>";
    return body;
}

static void Handler( struct mg_connection* nc, int ev, void* data )
{
    if( ev != MG_EV_HTTP_REQUEST ) return;
    auto hm = (struct http_message*)data;
    char remoteAddr[100];
    mg_sock_to_str( nc->sock, remoteAddr, sizeof(remoteAddr), MG_SOCK_STRINGIFY_REMOTE | MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT );
    std::string uri( hm->uri.p, hm->uri.len );
    auto parts = SplitPath( uri );

    int code = 200;
    std::string title, body;

    if( parts.size() == 2 && parts[0] == "msgid" )
    {
        const std::string& midStr = parts[1];
        bool found = false;
        if( midStr.size() <= 2048 && IsMsgId( midStr.c_str(), midStr.c_str() + midStr.size() ) )
        {
            uint8_t packed[4096];
            galaxy->PackMsgId( midStr.c_str(), packed );
            auto gidx = galaxy->GetMessageIndex( packed );
            if( gidx >= 0 )
            {
                auto groups = galaxy->GetGroups( gidx );
                for( uint64_t i=0; i<groups.size && !found; i++ )
                {
                    int aidx = groups.ptr[i];
                    if( galaxy->IsArchiveAvailable( aidx ) )
                    {
                        auto& archive = *galaxy->GetArchive( aidx, false );
                        uint8_t archivePacked[4096];
                        archive.RepackMsgId( packed, archivePacked, galaxy->GetCompress() );
                        int localIdx = archive.GetMessageIndex( archivePacked );
                        if( localIdx >= 0 )
                        {
                            std::string loc = "/group/" + UriPathEncode( galaxy->GetArchiveName( aidx ) ) + "/msg/" + std::to_string( localIdx );
                            mg_http_send_redirect( nc, 302, mg_mk_str( loc.c_str() ), mg_mk_str( "" ) );
                            found = true;
                        }
                    }
                }
            }
        }
        if( !found ) mg_http_send_error( nc, 404, nullptr );
        printf( "%s \"%.*s %.*s\" %i\n", remoteAddr, (int)hm->method.len, hm->method.p, (int)hm->uri.len, hm->uri.p, found ? 302 : 404 );
        fflush( stdout );
        return;
    }

    if( parts.empty() )
    {
        title = "Groups";
        body = RenderGroupList();
    }
    else if( parts[0] == "search" && parts.size() == 1 )
    {
        std::string q = GetQueryVar( hm->query_string, "q" );
        title = "Search";
        if( q.empty() )
        {
            body = RenderResults( {}, q, "/search", false );
        }
        else
        {
            std::vector<ResultRow> rows;
            for( int idx : galaxy->GetAvailableArchives() )
            {
                auto& archive = *galaxy->GetArchive( idx, false );
                auto part = SearchArchive( archive, galaxy->GetArchiveName( idx ), q, SearchGlobalCap );
                rows.insert( rows.end(), part.begin(), part.end() );
            }
            std::sort( rows.begin(), rows.end(), []( const ResultRow& a, const ResultRow& b ) { return a.rank > b.rank; } );
            bool truncated = rows.size() > SearchGlobalCap;
            if( truncated ) rows.resize( SearchGlobalCap );
            body = RenderResults( rows, q, "/search", truncated );
        }
    }
    else if( parts[0] == "group" && parts.size() >= 2 )
    {
        const std::string& name = parts[1];
        auto it = groupIndex.find( name );
        if( it == groupIndex.end() )
        {
            code = 404;
        }
        else
        {
            int archiveIdx = it->second;
            auto& archive = *galaxy->GetArchive( archiveIdx, false );

            if( parts.size() == 2 )
            {
                std::string pageStr = GetQueryVar( hm->query_string, "page" );
                size_t page = pageStr.empty() ? 0 : (size_t)strtoul( pageStr.c_str(), nullptr, 10 );
                title = name;
                body = RenderThreadList( name, archiveIdx, page );
            }
            else if( parts.size() == 3 && parts[2] == "search" )
            {
                std::string q = GetQueryVar( hm->query_string, "q" );
                title = name + " search";
                auto rows = SearchArchive( archive, name, q, SearchGlobalCap + 1 );
                bool truncated = rows.size() > SearchGlobalCap;
                if( truncated ) rows.resize( SearchGlobalCap );
                body = RenderResults( rows, q, "/group/" + Encode( name ) + "/search", truncated );
            }
            else if( parts.size() == 4 && parts[2] == "thread" )
            {
                char* end;
                uint32_t idx = (uint32_t)strtoul( parts[3].c_str(), &end, 10 );
                if( *end != '\0' || end == parts[3].c_str() || idx >= archive.NumberOfMessages() ) code = 404;
                else
                {
                    title = name;
                    body = RenderThread( name, archiveIdx, idx );
                }
            }
            else if( parts.size() == 4 && parts[2] == "msg" )
            {
                char* end;
                uint32_t idx = (uint32_t)strtoul( parts[3].c_str(), &end, 10 );
                if( *end != '\0' || end == parts[3].c_str() || idx >= archive.NumberOfMessages() ) code = 404;
                else
                {
                    title = name;
                    body = RenderSingleMessage( name, archiveIdx, idx );
                    if( body.empty() ) code = 404;
                }
            }
            else
            {
                code = 404;
            }
        }
    }
    else
    {
        code = 404;
    }

    int size = 0;
    if( code == 404 )
    {
        mg_http_send_error( nc, 404, nullptr );
    }
    else
    {
        std::string page = PageHeader( title.empty() ? "Usenet Archive" : title ) + body + PageFooter();
        size = (int)page.size();
        mg_send_head( nc, 200, size, "Content-Type: text/html; charset=utf-8" );
        mg_printf( nc, "%.*s", size, page.c_str() );
    }

    printf( "%s \"%.*s %.*s\" %i %i\n", remoteAddr, (int)hm->method.len, hm->method.p, (int)hm->uri.len, hm->uri.p, code, size );
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
    const char* port = "8120";
    const char* galaxyPath = "news/galaxy";

    TryIni( bind, config, "server", "bind" );
    TryIni( port, config, "server", "port" );
    TryIni( galaxyPath, config, "galaxy", "path" );

    galaxy.reset( Galaxy::Open( galaxyPath ) );
    if( !galaxy )
    {
        fprintf( stderr, "Cannot access galaxy at %s!\n", galaxyPath );
        fflush( stderr );
        ini_free( config );
        return 3;
    }

    for( int idx : galaxy->GetAvailableArchives() )
    {
        groupIndex[galaxy->GetArchiveName( idx )] = idx;
    }
    fprintf( stderr, "Loaded %zu groups.\n", groupIndex.size() );

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
