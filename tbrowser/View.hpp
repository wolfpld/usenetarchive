#ifndef __VIEW_HPP__
#define __VIEW_HPP__

#include <curses.h>

class View
{
public:
    View( int x, int y, int w, int h );
    virtual ~View();

    int GetKey();

protected:
    void ResizeView( int x, int y, int w, int h );

    WINDOW* m_win;
};

#endif
