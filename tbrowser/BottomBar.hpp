#ifndef __BOTTOMBAR_HPP__
#define __BOTTOMBAR_HPP__

#include <string>

#include "View.hpp"

class BottomBar : public View
{
public:
    BottomBar();

    void Update();
    void Resize() const;

    std::string Query( const char* prompt );

private:
    void PrintHelp() const;
    void PrintQuery( const char* prompt, const char* str ) const;

    int m_reset;
};

#endif
