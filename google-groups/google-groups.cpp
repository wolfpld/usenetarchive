// Based on https://github.com/henryk/gggd

#include <assert.h>
#include <curl/curl.h>
#include <mutex>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include "../common/Filesystem.hpp"
#include "../common/TaskDispatch.hpp"

#ifndef _MSC_VER
#include <pthread.h>
#include <openssl/err.h>

#define MUTEX_TYPE       pthread_mutex_t
#define MUTEX_SETUP(x)   pthread_mutex_init(&(x), NULL)
#define MUTEX_CLEANUP(x) pthread_mutex_destroy(&(x))
#define MUTEX_LOCK(x)    pthread_mutex_lock(&(x))
#define MUTEX_UNLOCK(x)  pthread_mutex_unlock(&(x))
#define THREAD_ID        pthread_self()


void handle_error(const char *file, int lineno, const char *msg)
{
    fprintf(stderr, "** %s:%d %s\n", file, lineno, msg);
    ERR_print_errors_fp(stderr);
    /* exit(-1); */ 
}

/* This array will store all of the mutexes available to OpenSSL. */ 
static MUTEX_TYPE *mutex_buf= NULL;

static void locking_function(int mode, int n, const char * file, int line)
{
    if(mode & CRYPTO_LOCK)
        MUTEX_LOCK(mutex_buf[n]);
    else
        MUTEX_UNLOCK(mutex_buf[n]);
}

static unsigned long id_function(void)
{
    return ((unsigned long)THREAD_ID);
}

int thread_setup(void)
{
    int i;

    mutex_buf = (pthread_mutex_t*)malloc(CRYPTO_num_locks() * sizeof(MUTEX_TYPE));
    if(!mutex_buf)
        return 0;
    for(i = 0;  i < CRYPTO_num_locks();  i++)
        MUTEX_SETUP(mutex_buf[i]);
    CRYPTO_set_id_callback(id_function);
    CRYPTO_set_locking_callback(locking_function);
    return 1;
}

int thread_cleanup(void)
{
    int i;

    if(!mutex_buf)
        return 0;
    CRYPTO_set_id_callback(NULL);
    CRYPTO_set_locking_callback(NULL);
    for(i = 0;  i < CRYPTO_num_locks();  i++)
        MUTEX_CLEANUP(mutex_buf[i]);
    free(mutex_buf);
    mutex_buf = NULL;
    return 1;
}
#endif

enum { Workers = 32 };
TaskDispatch td( Workers );

static std::mutex state;
static int pages = 0;
static bool pagesdone = false;
static int threads = 0;
static int threadstotal = 0;
static int threadsbad = 0;
static int messages = 0;
static int messagestotal = 0;
static int messagesbad = 0;

static std::string base;

static void PrintState( const char* group )
{
    char buf[32];
    if( !pagesdone )
    {
        sprintf( buf, "+" );
    }
    else
    {
        sprintf( buf, "/%i", pages );
    }
    printf( "\r%s .:. [P]: %i%s .:. [T]: %i/%i (-%i) .:. [M]: %i/%i (-%i)", group, pages, buf, threads, threadstotal, threadsbad, messages, messagestotal, messagesbad );
    fflush( stdout );
}

static size_t WriteFn( void* _data, size_t size, size_t num, void* ptr )
{
    const auto data = (unsigned char*)_data;
    const auto sz = size*num;
    auto& v = *(std::vector<unsigned char>*)ptr;
    v.insert( v.end(), data, data+sz );
    return sz;
}

static std::vector<unsigned char> Fetch( const std::string& url )
{
    std::vector<unsigned char> buf;
    buf.reserve( 128 * 1024 );
    auto curl = curl_easy_init();
    if( curl )
    {
        curl_easy_setopt( curl, CURLOPT_URL, url.c_str() );
        curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1L );
        curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, WriteFn );
        curl_easy_setopt( curl, CURLOPT_WRITEDATA, &buf );
        auto res = curl_easy_perform( curl );
        if( res != CURLE_OK )
        {
            buf.clear();
        }
        else
        {
            buf.emplace_back( '\0' );
        }
        curl_easy_cleanup( curl );
    }
    return buf;
}

static void GetMsg( const std::string& msgid, const char* group, int len )
{
    std::string url( "https://groups.google.com/forum/message/raw?msg=" );
    url += group;
    url.push_back( '/' );
    url += msgid;
    auto buf = Fetch( url );
    if( buf.empty() || buf[0] == '<' )
    {
        std::lock_guard<std::mutex> lock( state );
        messages++;
        messagesbad++;
        PrintState( group );
        return;
    }
    assert( buf.back() == '\0' );
    buf.pop_back();

    auto str = base;
    str += msgid;

    FILE* f = fopen( str.c_str(), "wb" );
    if( f )
    {
        fwrite( buf.data(), 1, buf.size(), f );
        fclose( f );
    }

    std::lock_guard<std::mutex> lock( state );
    messages++;
    PrintState( group );
}

static void GetThread( const std::string& id, const char* group, int len )
{
    std::string url( "https://groups.google.com/forum/?_escaped_fragment_=topic/" );
    url += group;
    url.push_back( '/' );
    url += id;
    auto buf = Fetch( url );
    if( buf.empty() )
    {
        std::lock_guard<std::mutex> lock( state );
        threads++;
        threadsbad++;
        PrintState( group );
        return;
    }
    auto ptr = (const char*)buf.data();
    int nummsg = 0;

    auto str = base;
    str += id;
    CreateDirStruct( str );

    while( *ptr )
    {
        if( strncmp( ptr, "https://groups.google.com/d/msg/", 32 ) == 0 )
        {
            ptr += 33 + len;
            auto end = ptr;
            while( *end != '"' ) end++;
            std::string msgid( ptr, end );
            td.Queue( [msgid, group, len] {
                GetMsg( msgid, group, len );
            } );
            ptr = end + 1;
            nummsg++;
        }
        else
        {
            ptr++;
        }
    }
    std::lock_guard<std::mutex> lock( state );
    messagestotal += nummsg;
    threads++;
    PrintState( group );
}

void GetTopics( const std::string& url, const char* group, int len )
{
    auto buf = Fetch( url );
    if( buf.empty() ) return;
    auto ptr = (const char*)buf.data();
    int numthr = 0;
    bool done = true;
    while( *ptr )
    {
        if( strncmp( ptr, "/topic/", 7 ) == 0 )
        {
            ptr += 8 + len;
            auto end = ptr;
            while( *end != '"' ) end++;
            std::string id( ptr, end );
            td.Queue( [id, group, len] {
                GetThread( id, group, len );
            } );
            ptr = end + 1;
            numthr++;
        }
        else if( strncmp( ptr, "_escaped_fragment_", 18 ) == 0 )
        {
            auto begin = ptr - 33;
            auto end = ptr + 18;
            while( *end != '"' ) end++;
            std::string url( begin, end );
            td.Queue( [url, group, len] {
                GetTopics( url, group, len );
            } );
            ptr = end + 1;
            done = false;
        }
        else
        {
            ptr++;
        }
    }
    std::lock_guard<std::mutex> lock( state );
    threadstotal += numthr;
    pages++;
    pagesdone = done;
    PrintState( group );
}

int main( int argc, char** argv )
{
    if( argc != 2 )
    {
        fprintf( stderr, "USAGE: %s group\n", argv[0] );
        exit( 1 );
    }
    if( Exists( argv[1] ) )
    {
        fprintf( stderr, "Destination directory already exists.\n" );
        exit( 1 );
    }

#ifndef _MSC_VER
    thread_setup();
#endif

    if( curl_global_init( CURL_GLOBAL_ALL ) != 0 )
    {
        fprintf( stderr, "Cannot initialize libcurl.\n" );
        exit( 1 );
    }

    CreateDirStruct( argv[1] );
    base = argv[1];
    base.append( "/" );

    std::string startUrl( "https://groups.google.com/forum/?_escaped_fragment_=forum/" );
    startUrl += argv[1];
    td.Queue( [startUrl, argv] {
        GetTopics( startUrl, argv[1], strlen( argv[1] ) );
    } );
    td.Sync();
    printf( "\n" );

#ifndef _MSC_VER
    thread_cleanup();
#endif

    return 0;
}
