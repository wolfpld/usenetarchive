#ifndef __NAMED_MUTEX_HPP__
#define __NAMED_MUTEX_HPP__

#include <assert.h>

#ifdef _MSC_VER

#  include <windows.h>

class named_mutex
{
public:
    named_mutex( const char* name )
        : m_hnd( CreateMutex( nullptr, FALSE, name ) )
    {
    }

    ~named_mutex()
    {
        CloseHandle( m_hnd );
    }

    void lock()
    {
        WaitForSingleObject( m_hnd, INFINITE );
    }

    void unlock()
    {
        ReleaseMutex( m_hnd );
    }

    bool try_lock()
    {
        switch( WaitForSingleObject( m_hnd, 0 ) )
        {
        case WAIT_OBJECT_0:
            return true;
        case WAIT_TIMEOUT:
            return false;
        case WAIT_ABANDONED:
        case WAIT_FAILED:
        default:
            assert( false );
            return false;
        }
    }

private:
    HANDLE m_hnd;
};

#  undef GetMessage

#else

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <string.h>

class named_mutex
{
public:
    named_mutex( const char* name )
    {
        assert( name && *name );
        const auto size = strlen( name );
        auto tmp = std::make_unique<char[]>( size + 2 );
        tmp[0] = '/';
        memcpy( tmp.get() + 1, name, size );
        tmp[size+1] = '\0';
        for( int i=1; i<size+1; i++ )
        {
            if( tmp[i] == '/' ) tmp[i] = ':';
        }

        m_sem = sem_open( tmp, O_CREAT, S_IRUSR | S_IWUSR, 1 );
    }

    ~named_mutex()
    {
        sem_close( m_sem );
    }

    void lock()
    {
        sem_wait( m_sem );
    }

    void unlock()
    {
        sem_post( m_sem );
    }

    bool try_lock()
    {
        if( sem_trywait( m_sem ) == 0 ) return true;
        switch( errno )
        {
        case EINTR:
        case EAGAIN:
            return false;
        default:
            assert( false );
            return false;
        }
    }

private:
    sem_t* m_sem;
};

#endif

#endif
