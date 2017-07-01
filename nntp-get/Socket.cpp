#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "Socket.hpp"

#ifdef _MSC_VER
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#  include <netdb.h>
#endif

Socket::Socket()
    : m_sock( -1 )
{
#ifdef _MSC_VER
    static bool initDone = false;
    if( !initDone )
    {
        initDone = true;
        WSADATA wsaData;
        if( WSAStartup( MAKEWORD( 2, 2 ), &wsaData ) != 0 )
        {
            fprintf( stderr, "Cannot init winsock.\n" );
            exit( 1 );
        }
    }
#endif
}

Socket::~Socket()
{
    if( m_sock != -1 )
    {
        Close();
    }
}

bool Socket::Connect( const char* addr )
{
    struct addrinfo hints;
    struct addrinfo *res, *ptr;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if( getaddrinfo( addr, "119", &hints, &res ) != 0 ) return false;
    int sock;
    for( ptr = res; ptr; ptr = ptr->ai_next )
    {
        if( ( sock = socket( ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol ) ) == -1 ) continue;
        if( connect( sock, ptr->ai_addr, ptr->ai_addrlen ) == -1 )
        {
#ifdef _MSC_VER
            closesocket( sock );
#else
            close( sock );
#endif
            continue;
        }
        break;
    }
    freeaddrinfo( res );
    if( !ptr ) return false;

    m_sock = sock;
}

void Socket::Close()
{
    assert( m_sock != -1 );
#ifdef _MSC_VER
    closesocket( m_sock );
#else
    close( m_sock );
#endif
    m_sock = -1;
}

int Socket::Send( const char* buf, int len )
{
    assert( m_sock != -1 );
    auto start = buf;
    while( len > 0 )
    {
        auto ret = send( m_sock, buf, len, 0 );
        if( ret == -1 ) return -1;
        len -= ret;
        buf += ret;
    }
    return buf - start;
}

int Socket::Recv( char* buf, int len )
{
    assert( m_sock != -1 );
    int size;
    do
    {
        size = recv( m_sock, buf, len, 0 );
    }
    while( size == -1 );
    return size;
}
