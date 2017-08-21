#ifndef __CHARTVIEW_HPP__
#define __CHARTVIEW_HPP__

#include "View.hpp"

class Browser;

class ChartView : public View
{
public:
    ChartView( Browser* parent );

    void Entry();

    void Resize();
    void Draw();

private:
    Browser* m_parent;
    bool m_active;
};

#endif
