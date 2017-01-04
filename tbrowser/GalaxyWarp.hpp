#ifndef __GALAXYWARP_HPP__
#define __GALAXYWARP_HPP__

#include <vector>

#include "View.hpp"

class BottomBar;
class Browser;
class Galaxy;
class PersistentStorage;

class GalaxyWarp : public View
{
public:
    GalaxyWarp( Browser* parent, BottomBar& bar, Galaxy& galaxy, PersistentStorage& storage );

    void Entry();

    void Resize();
    void Draw();

private:
    void MoveCursor( int offset );

    Browser* m_parent;
    BottomBar& m_bar;
    Galaxy& m_galaxy;
    PersistentStorage& m_storage;

    bool m_active;

    int m_cursor;
    int m_top, m_bottom;
};

#endif
