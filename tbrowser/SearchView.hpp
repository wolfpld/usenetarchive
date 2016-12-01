#ifndef __SEARCHVIEW_HPP__
#define __SEARCHVIEW_HPP__

#include <string>

#include "View.hpp"

class BottomBar;
class Browser;

class SearchView : public View
{
public:
    SearchView( Browser* parent, BottomBar& bar );

    void Entry();

    void Resize();
    void Draw();

private:
    Browser* m_parent;
    BottomBar& m_bar;
    std::string m_query;
    bool m_active;
};

#endif
