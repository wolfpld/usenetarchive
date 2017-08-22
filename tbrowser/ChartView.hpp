#ifndef __CHARTVIEW_HPP__
#define __CHARTVIEW_HPP__

#include <vector>

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
    void Prepare();

    Browser* m_parent;
    bool m_active;

    std::vector<uint16_t> m_data;
    std::vector<char[7]> m_label;
    uint32_t m_max;
    uint8_t m_hires;
};

#endif
