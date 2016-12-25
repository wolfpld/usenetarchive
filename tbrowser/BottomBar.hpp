#ifndef __BOTTOMBAR_HPP__
#define __BOTTOMBAR_HPP__

#include <string>

#include "View.hpp"

class Browser;

enum class HelpSet
{
    Default,
    Search,
    Text
};

class BottomBar : public View
{
public:
    BottomBar( Browser* parent );

    void Update();
    void Resize() const;

    std::string Query( const char* prompt, const char* entry = nullptr );
    void Status( const char* status, int timeout = 2 );
    void SetHelp( HelpSet set );

private:
    void PrintHelp() const;
    void PrintQuery( const char* prompt, const char* str ) const;

    Browser* m_parent;
    int m_reset;
    HelpSet m_help;
};

#endif
