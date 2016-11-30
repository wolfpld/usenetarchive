#ifndef __SEARCHVIEW_HPP__
#define __SEARCHVIEW_HPP__

#include "View.hpp"

class SearchView : public View
{
public:
    SearchView();

    void Resize();
    void Draw();

private:
    bool m_active;
};

#endif
