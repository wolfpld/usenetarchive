#ifndef __BOTTOMBAR_HPP__
#define __BOTTOMBAR_HPP__

#include "View.hpp"

class BottomBar : public View
{
public:
    BottomBar();

    void Resize() const;

private:
    void PrintHelp() const;
};

#endif
