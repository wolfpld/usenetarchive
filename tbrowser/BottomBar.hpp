#ifndef __BOTTOMBAR_HPP__
#define __BOTTOMBAR_HPP__

#include <string>

#include "View.hpp"

class Browser;

class BottomBar : public View
{
public:
    BottomBar( Browser* parent );

    void Update();
    void Resize() const;

    std::string Query( const char* prompt );
    void Status( const char* status );

private:
    void PrintHelp() const;
    void PrintQuery( const char* prompt, const char* str ) const;

    Browser* m_parent;
    int m_reset;
};

#endif
