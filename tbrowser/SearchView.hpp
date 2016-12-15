#ifndef __SEARCHVIEW_HPP__
#define __SEARCHVIEW_HPP__

#include <string>
#include <vector>

#include "../libuat/Archive.hpp"

#include "View.hpp"

class BottomBar;
class Browser;

class SearchView : public View
{
public:
    SearchView( Browser* parent, BottomBar& bar, Archive& archive, PersistentStorage& storage );

    void Entry();

    void Resize();
    void Draw();

private:
    void FillPreview( int idx );
    void MoveCursor( int offset );

    Browser* m_parent;
    BottomBar& m_bar;
    Archive& m_archive;
    PersistentStorage& m_storage;
    std::string m_query;
    SearchData m_result;
    std::vector<std::string> m_preview;
    bool m_active;

    int m_top, m_bottom;
    int m_cursor;
};

#endif
