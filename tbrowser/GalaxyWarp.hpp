#ifndef __GALAXYWARP_HPP__
#define __GALAXYWARP_HPP__

#include <stdint.h>
#include <vector>

#include "GalaxyState.hpp"
#include "View.hpp"

class BottomBar;
class Browser;
class Galaxy;
class PersistentStorage;

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

class GalaxyWarp : public View
{
public:
    GalaxyWarp( Browser* parent, BottomBar& bar, Galaxy& galaxy, PersistentStorage& storage );

    void Entry( const char* msgid, GalaxyState state );

    void Resize();
    void Draw();

private:
    void MoveCursor( int offset );

    Browser* m_parent;
    BottomBar& m_bar;
    Galaxy& m_galaxy;
    PersistentStorage& m_storage;

    std::vector<WarpEntry> m_list;

    bool m_active;

    int m_cursor;
    int m_top, m_bottom;

    const char* m_msgid;
};

#endif
