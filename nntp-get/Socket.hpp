#ifndef __SOCKET_HPP__
#define __SOCKET_HPP__

class Socket
{
public:
    Socket();
    ~Socket();

    bool Connect( const char* addr );
    void Close();

    int Send( const char* buf, int len );
    int Recv( char* buf, int len );

private:
    int m_sock;
};

#endif
