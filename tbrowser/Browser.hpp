#ifndef __BROWSER_HPP__
#define __BROWSER_HPP__

#include <memory>

#include "BottomBar.hpp"
#include "HeaderBar.hpp"
#include "MessageView.hpp"
#include "ThreadView.hpp"

class Archive;
class PersistentStorage;

class Browser
{
public:
    Browser( std::unique_ptr<Archive>&& archive, PersistentStorage& storage, const char* fn );

    void Entry();

private:
    bool MoveOrEnterAction( int move );
    void SwitchToMessage( int msgidx );

    std::unique_ptr<Archive> m_archive;
    PersistentStorage& m_storage;

    HeaderBar m_header;
    BottomBar m_bottom;
    MessageView m_mview;
    ThreadView m_tview;

    const char* m_fn;
    int m_historyIdx;
};

#endif
