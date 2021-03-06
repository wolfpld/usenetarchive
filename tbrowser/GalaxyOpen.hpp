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
    GalaxyOpen( Browser* parent, BottomBar& bar, Galaxy& galaxy );

    void Entry();

    void Resize();
    void Draw();

private:
    void MoveCursor( int offset );
    void FilterItems( const std::string& filter );

    int CalcOffset( int offset );

    Browser* m_parent;
    BottomBar& m_bar;
    Galaxy& m_galaxy;
    std::vector<bool> m_filter;

    bool m_active;

    int m_cursor;
    int m_top, m_bottom;
};

#endif
