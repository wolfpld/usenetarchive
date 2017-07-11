#ifndef __GALAXYWARP_HPP__
#define __GALAXYWARP_HPP__

#include <map>
#include <stdint.h>
#include <vector>

#include "GalaxyState.hpp"
#include "View.hpp"

class BottomBar;
class Browser;
class Galaxy;
class PersistentStorage;
class ThreadTree;

struct WarpEntry
{
    uint32_t id;
    bool available;
    bool current;
    bool indirect;
    const char* msgid;
    int parent;
    int children;
    int totalchildren;
};

struct PreviewEntry
{
    int treeid;
    int begin;
    int end;
    int idx;
};

class GalaxyWarp : public View
{
public:
    GalaxyWarp( Browser* parent, BottomBar& bar, Galaxy& galaxy, PersistentStorage& storage );

    void Entry( const char* msgid, GalaxyState state, bool showIndirect );
    void Resize();

private:
    void MoveCursor( int offset );

    void Draw();
    void DrawPreview( int size );

    void Cleanup();
    void PreparePreview( int cursor );

    Browser* m_parent;
    BottomBar& m_bar;
    Galaxy& m_galaxy;
    PersistentStorage& m_storage;

    std::vector<WarpEntry> m_list;
    std::vector<PreviewEntry> m_preview;
    std::vector<ThreadTree> m_treeCache;
    std::map<uint32_t, uint32_t> m_treeCacheMap;

    bool m_active;

    int m_cursor;
    int m_top, m_bottom;

    const char* m_msgid;
};

#endif
