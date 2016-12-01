#ifndef __SEARCHVIEW_HPP__
#define __SEARCHVIEW_HPP__

#include <string>
#include <vector>

#include "../libuat/Archive.hpp"

#include "View.hpp"

class Archive;
class BottomBar;
class Browser;

class SearchView : public View
{
public:
    SearchView( Browser* parent, BottomBar& bar, Archive& archive );

    void Entry();

    void Resize();
    void Draw();

private:
    Browser* m_parent;
    BottomBar& m_bar;
    Archive& m_archive;
    std::string m_query;
    std::vector<SearchResult> m_result;
    bool m_active;
};

#endif
