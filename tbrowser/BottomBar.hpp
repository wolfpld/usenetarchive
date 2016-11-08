#ifndef __BOTTOMBAR_HPP__
#define __BOTTOMBAR_HPP__

#include "View.hpp"

class BottomBar : public View
{
public:
    BottomBar( int total );

    void Update( int index );

private:
    int m_total;
};

#endif
