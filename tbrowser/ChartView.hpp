#ifndef __CHARTVIEW_HPP__
#define __CHARTVIEW_HPP__

#include <vector>

#include "View.hpp"

class Archive;
class BottomBar;
class Browser;
class SearchEngine;

class ChartView : public View
{
public:
    ChartView( Browser* parent, Archive& archive, BottomBar& bar );

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

    std::vector<uint32_t> m_posts;
    std::vector<uint16_t> m_data;
    std::vector<char[7]> m_label;
    uint32_t m_max;
    uint8_t m_hires;
};

#endif
