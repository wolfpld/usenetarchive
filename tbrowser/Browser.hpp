#ifndef __BROWSER_HPP__
#define __BROWSER_HPP__

#include <memory>
#include <string>

#include "BottomBar.hpp"
#include "HeaderBar.hpp"
#include "MessageView.hpp"
#include "TextView.hpp"
#include "ThreadView.hpp"
#include "SearchView.hpp"

class Archive;
class PersistentStorage;

class Browser
{
public:
    Browser( std::unique_ptr<Archive>&& archive, PersistentStorage& storage, const std::string& fn );

    void Entry();
    void Resize();

    void OpenMessage( int msgidx );

    const char* GetArchiveFilename() const { return m_fn.c_str(); }

private:
    bool MoveOrEnterAction( int move );
    void SwitchToMessage( int msgidx );
    void RestoreDefaultView();

    std::unique_ptr<Archive> m_archive;
    PersistentStorage& m_storage;

    HeaderBar m_header;
    BottomBar m_bottom;
    MessageView m_mview;
    ThreadView m_tview;
    SearchView m_sview;
    TextView m_textview;

    std::string m_fn;
    int m_historyIdx;
};

#endif
