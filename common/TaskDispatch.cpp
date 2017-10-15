#include <assert.h>
#include <stdio.h>

#include "System.hpp"
#include "TaskDispatch.hpp"

TaskDispatch::TaskDispatch( size_t workers )
    : m_exit( false )
    , m_jobs( 0 )
{
    assert( workers >= 1 );
    workers--;

#ifdef __CYGWIN__
    if( workers > 7 ) workers = 7;
#endif

    m_workers.reserve( workers );
    for( size_t i=0; i<workers; i++ )
    {
        char tmp[16];
        sprintf( tmp, "Worker %zu", i );
        auto worker = std::thread( [this]{ Worker(); } );
        System::SetThreadName( worker, tmp );
        m_workers.emplace_back( std::move( worker ) );
    }
}

TaskDispatch::~TaskDispatch()
{
    m_exit = true;
    m_cvWork.notify_all();

    for( auto& worker : m_workers )
    {
        worker.join();
    }
}

void TaskDispatch::Queue( const std::function<void(void)>& f )
{
    std::unique_lock<std::mutex> lock( m_queueLock );
    m_queue.emplace( f );
    const auto size = m_queue.size();
    lock.unlock();
    if( size > 1 )
    {
        m_cvWork.notify_one();
    }
}

void TaskDispatch::Queue( std::function<void(void)>&& f )
{
    std::unique_lock<std::mutex> lock( m_queueLock );
    m_queue.emplace( std::move( f ) );
    const auto size = m_queue.size();
    lock.unlock();
    if( size > 1 )
    {
        m_cvWork.notify_one();
    }
}

void TaskDispatch::Sync()
{
    std::unique_lock<std::mutex> lock( m_queueLock );
    while( !m_queue.empty() )
    {
        auto f = m_queue.front();
        m_queue.pop();
        lock.unlock();
        f();
        lock.lock();
    }
    m_cvJobs.wait( lock, [this]{ return m_jobs == 0; } );
}

void TaskDispatch::Worker()
{
    for(;;)
    {
        std::unique_lock<std::mutex> lock( m_queueLock );
        m_cvWork.wait( lock, [this]{ return !m_queue.empty() || m_exit; } );
        if( m_exit ) return;
        auto f = m_queue.front();
        m_queue.pop();
        m_jobs++;
        lock.unlock();
        f();
        lock.lock();
        m_jobs--;
        bool notify = m_jobs == 0 && m_queue.empty();
        lock.unlock();
        if( notify )
        {
            m_cvJobs.notify_all();
        }
    }
}
