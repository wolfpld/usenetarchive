#ifndef __TEXTVIEW_HPP__
#define __TEXTVIEW_HPP__

#include <string>
#include <vector>

#include "View.hpp"

class Browser;

class TextView : public View
{
public:
    TextView( Browser* parent );

    void Entry();

    void Resize();
    void Draw();

private:
    Browser* m_parent;
    bool m_active;
};

#endif
