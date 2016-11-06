#ifndef __VIEW_HPP__
#define __VIEW_HPP__

#include <curses.h>

class View
{
public:
    View( int x, int y, int w, int h );
    virtual ~View();

protected:
    WINDOW* m_win;
};

#endif
