#ifndef __SEARCHVIEW_HPP__
#define __SEARCHVIEW_HPP__

#include <memory>
#include <string>
#include <vector>

#include "../common/ExpandingBuffer.hpp"
#include "../libuat/SearchEngine.hpp"

#include "View.hpp"

class BottomBar;
class Browser;
class PersistentStorage;

class SearchView : public View
{
public:
    SearchView( Browser* parent, BottomBar& bar, Archive& archive, PersistentStorage& storage );

    void Entry();

    void Resize();
    void Draw();

    void Reset( Archive& archive );

private:
    struct PreviewData
    {
        std::string text;
        chtype color;
        bool newline;
    };

    void FillPreview( int idx );
    void MoveCursor( int offset );

    ExpandingBuffer m_eb;
    Browser* m_parent;
    BottomBar& m_bar;
    Archive* m_archive;
    std::unique_ptr<SearchEngine> m_search;
    PersistentStorage& m_storage;
    std::string m_query;
    float m_queryTime;
    SearchData m_result;
    std::vector<std::vector<PreviewData>> m_preview;
    bool m_active;

    int m_top, m_bottom;
    int m_cursor;
};

#endif
