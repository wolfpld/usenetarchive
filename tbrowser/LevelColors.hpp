#ifndef __LEVELCOLORS_HPP__
#define __LEVELCOLORS_HPP__

#include <curses.h>

// colors for quotation levels
static const chtype QuoteFlags[] = {
    COLOR_PAIR( 5 ),            // T_Quote1
    COLOR_PAIR( 3 ),            // T_Quote2
    COLOR_PAIR( 6 ) | A_BOLD,   // T_Quote3
    COLOR_PAIR( 2 ),            // T_Quote4
    COLOR_PAIR( 4 ),            // T_Quote5
};

enum { NumQuoteFlags = sizeof( QuoteFlags ) / sizeof( chtype ) };

#endif
