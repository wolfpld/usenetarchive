#ifndef __ICU_HPP__
#define __ICU_HPP__

#include <string>
#include <vector>

void SplitLine( const char* ptr, const char* end, std::vector<std::string>& out, bool toLower = true );
std::string ToLower( const char* ptr, const char* end );

#endif
