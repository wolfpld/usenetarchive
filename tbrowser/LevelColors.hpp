#ifndef __LEVELCOLORS_HPP__
#define __LEVELCOLORS_HPP__

#include <curses.h>

static const chtype QuoteFlags[] = {
    COLOR_PAIR( 5 ),
    COLOR_PAIR( 3 ),
    COLOR_PAIR( 6 ) | A_BOLD,
    COLOR_PAIR( 2 ),
    COLOR_PAIR( 4 )
};

#endif
