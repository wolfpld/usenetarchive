#ifndef __SEARCHVIEW_HPP__
#define __SEARCHVIEW_HPP__

#include <string>
#include <vector>

#include "View.hpp"

class Archive;
class BottomBar;
class Browser;
class PersistentStorage;
class SearchResult;

class SearchView : public View
{
public:
    SearchView( Browser* parent, BottomBar& bar, Archive& archive, PersistentStorage& storage );

    void Entry();

    void Resize();
    void Draw();

private:
    void FillPreview( int idx );

    Browser* m_parent;
    BottomBar& m_bar;
    Archive& m_archive;
    PersistentStorage& m_storage;
    std::string m_query;
    std::vector<SearchResult> m_result;
    std::vector<std::string> m_preview;
    bool m_active;
};

#endif
