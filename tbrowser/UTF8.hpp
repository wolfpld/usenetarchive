#ifndef __UTF8_HPP__
#define __UTF8_HPP__

#include <stdlib.h>

size_t utflen( const char* str );
const char* utfend( const char* str, size_t& len );

#endif
