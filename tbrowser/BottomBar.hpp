#ifndef __BOTTOMBAR_HPP__
#define __BOTTOMBAR_HPP__

#include <functional>
#include <string>

#include "View.hpp"

class Browser;

enum class HelpSet
{
    Default,
    Search,
    Text,
    GalaxyOpen
};

class BottomBar : public View
{
public:
    BottomBar( Browser* parent );

    void Update();
    void Resize() const;

    std::string Query( const char* prompt, const char* entry = nullptr, bool filesystem = false );
    std::string InteractiveQuery( const char* prompt, const std::function<void(const std::string&)>& cb, const char* entry = nullptr );
    int KeyQuery( const char* prompt );
    void Status( const char* status, int timeout = 2 );
    void SetHelp( HelpSet set );

private:
    void PrintHelp() const;
    void PrintQuery( const char* prompt, const char* str ) const;
    void SuggestFiles( std::string& str, int& pos ) const;

    Browser* m_parent;
    int m_reset;
    HelpSet m_help;
};

#endif
