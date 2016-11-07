#ifndef __THREADVIEW_HPP__
#define __THREADVIEW_HPP__

#include <stdint.h>
#include <vector>

#include "View.hpp"

class Archive;

struct ThreadData
{
    unsigned int expanded   : 1;
    unsigned int valid      : 1;
    unsigned int msgid      : 30;
};

static_assert( sizeof( ThreadData ) == sizeof( uint32_t ), "Wrong size of ThreadData" );

class ThreadView : public View
{
public:
    ThreadView( const Archive& archive );
    ~ThreadView();

private:
    std::vector<ThreadData> m_data;
};

#endif
