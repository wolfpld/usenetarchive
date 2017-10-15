#ifndef __VIEW_HPP__
#define __VIEW_HPP__

#include <curses.h>

class View
{
public:
    View( int x, int y, int w, int h );
    virtual ~View();

    int GetKey() const;
    int GetHeight() const { return getmaxy( m_win ); }

protected:
    void ResizeView( int x, int y, int w, int h ) const;

    WINDOW* m_win;
};

#endif
