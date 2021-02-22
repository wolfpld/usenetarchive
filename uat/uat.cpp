#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const char* s_help =
"Usenet Archive Toolkit\n\n"
"Created by Bartosz Taudul <wolf@nereid.pl>\n"
"https://github.com/wolfpld/usenetarchive/\n"
"Licensed under GNU AGPL3\n\n"
"Usage: uat <command> [<args>]\n\n"
"The following commands are available:\n";

struct Tool
{
    const char* image;
    const char* desc;
};

static const Tool tools[] = {
    { "connectivity", "Calculate message dependency graph." },
    { "export-messages", "Save each messages in LZ4 workset to a file." },
    { "extract-msgid", "Build Message ID reference table." },
    { "extract-msgmeta", "Extract \"From\" and \"Subject\" fields." },
    { "filter-newsgroups", "Remove messages with wrong \"Newsgroups\" field." },
    { "filter-spam", "Learn which messages are spam and remove them." },
    { "galaxy-util", "Generate archive galaxy data." },
    { "google-groups", "Crawl google groups and save in maildir tree." },
    { "import-source-maildir", "Import messages from a directory tree." },
    { "import-source-maildir-7z", "Import messages from a compressed directory tree." },
    { "import-source-mbox", "Import messages from mbox archive." },
    { "kill-duplicates", "Remove duplicated messages." },
    { "lexdist", "Calculate distance between words." },
    { "lexicon", "Create search lexicon." },
    { "lexsort", "Sort lexicon data." },
    { "lexstats", "Show lexicon statistics." },
    { "merge-raw", "Merge two data sets into one." },
    { "nntp-get", "Get messages from NNTP server." },
    { "package", "Pack (or unpack) archive files into single file package." },
    { "query", "Perform queries on a final zstd data." },
    { "query-raw", "Perform queries on a LZ4 workset data." },
    { "relative-complement", "Create archive with messages unique to one archive." },
    { "repack-lz4", "Recompress zstd data to workset LZ4 format." },
    { "repack-zstd", "Recompress LZ4 data to the final zstd format." },
    { "sort", "Sort messages in thread-chronological order." },
    { "tbrowser", "Curses-based text mode archive browser." },
    { "threadify", "Find missing connections between messages." },
    { "update-zstd", "Update zstd archive." },
    { "utf8ize", "Perform conversion to UTF-8." },
    { "verify", "Check archive for known problems." },
};

void PrintHelp()
{
    printf( s_help );

    for( auto& tool : tools )
    {
        printf( " %-25s %s\n", tool.image, tool.desc );
    }

    printf( "\nRefer to uat-command man pages for more help.\n" );
}

int main( int argc, char** argv )
{
    if( argc == 1 )
    {
        PrintHelp();
        return 0;
    }

    char tmp[1024];
    sprintf( tmp, "%s/%s", "/usr/lib/uat/", argv[1] );

    char fn[1024];
    sprintf( fn, "uat %s", argv[1] );
    auto old = argv[1];
    argv[1] = fn;

    execv( tmp, argv+1 );

    fprintf( stderr, "No such command: %s\n", old );
    exit( 1 );
}
