#ifndef __HEADERBAR_HPP__
#define __HEADERBAR_HPP__

#include "View.hpp"

class HeaderBar : public View
{
public:
    HeaderBar( const char* archive, const char* desc );

    void Resize();

private:
    void Redraw();

    const char* m_archive;
    const char* m_desc;
};

#endif
