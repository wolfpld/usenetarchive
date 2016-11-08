#ifndef __BROWSER_HPP__
#define __BROWSER_HPP__

#include <memory>

#include "../libuat/Archive.hpp"

#include "BottomBar.hpp"
#include "HeaderBar.hpp"
#include "ThreadView.hpp"

class Browser
{
public:
    Browser( std::unique_ptr<Archive>&& archive );
    ~Browser();

    void Entry();

private:
    std::unique_ptr<Archive> m_archive;

    HeaderBar m_header;
    BottomBar m_bottom;
    ThreadView m_tview;
};

#endif
