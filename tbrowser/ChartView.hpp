#ifndef __CHARTVIEW_HPP__
#define __CHARTVIEW_HPP__

#include <vector>

#include "View.hpp"

class Archive;
class BottomBar;
class Browser;
class SearchEngine;
class Galaxy;

class ChartView : public View
{
public:
    ChartView( Browser* parent, Archive& archive, BottomBar& bar, Galaxy* galaxy );

    void Entry();

    void Resize();
    void Draw();

    void Reset( Archive& archive );

private:
    void Prepare();

    Browser* m_parent;
    bool m_active;

    Archive* m_archive;
    std::unique_ptr<SearchEngine> m_search;
    std::string m_query;
    BottomBar& m_bar;
    Galaxy* m_galaxy;

    std::vector<uint32_t> m_posts;
    std::vector<uint16_t> m_data;
    std::vector<uint16_t> m_trendData;
    std::vector<char[7]> m_label;
    uint32_t m_max;
    bool m_hires;
    bool m_trend;
    bool m_galaxyMode;
};

#endif
