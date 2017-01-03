#ifndef __GALAXYOPEN_HPP__
#define __GALAXYOPEN_HPP__

#include <vector>

#include "View.hpp"

class BottomBar;
class Browser;
class Galaxy;
class PersistentStorage;

class GalaxyOpen : public View
{
public:
    GalaxyOpen( Browser* parent, BottomBar& bar, Galaxy& galaxy, PersistentStorage& storage );

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

    int m_top, m_bottom;
    int m_cursor;
};

#endif
