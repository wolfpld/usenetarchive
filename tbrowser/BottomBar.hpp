#ifndef __BOTTOMBAR_HPP__
#define __BOTTOMBAR_HPP__

#include "View.hpp"

class BottomBar : public View
{
public:
    BottomBar( int total );

    void Resize( int index ) const;
    void Update( int index ) const;

private:
    int m_total;
};

#endif
