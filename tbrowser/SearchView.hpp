#ifndef __SEARCHVIEW_HPP__
#define __SEARCHVIEW_HPP__

#include "View.hpp"

class Browser;

class SearchView : public View
{
public:
    SearchView( Browser* parent );

    void Entry();

    void Resize();
    void Draw();

private:
    Browser* m_parent;
    bool m_active;
};

#endif
