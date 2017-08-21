#ifndef __BROWSER_HPP__
#define __BROWSER_HPP__

#include <memory>
#include <string>

#include "BottomBar.hpp"
#include "ChartView.hpp"
#include "HeaderBar.hpp"
#include "MessageView.hpp"
#include "TextView.hpp"
#include "ThreadView.hpp"
#include "SearchView.hpp"

class Archive;
class Galaxy;
class GalaxyOpen;
class GalaxyWarp;
class PersistentStorage;

class Browser
{
public:
    Browser( std::shared_ptr<Archive>&& archive, PersistentStorage& storage, Galaxy* galaxy, const std::string& fn );
    ~Browser();

    void Entry();
    void Resize();

    void OpenMessage( int msgidx );
    void SwitchToMessage( int msgidx );

    const char* GetArchiveFilename() const { return m_fn.c_str(); }

    void SwitchArchive( const std::shared_ptr<Archive>& archive, std::string&& fn );
    void DisplayTextView( const char* text, int size = -1 );

private:
    bool MoveOrEnterAction( int move );
    void RestoreDefaultView();
    void OpenArchive( std::string&& fn );

    std::shared_ptr<Archive> m_archive;
    PersistentStorage& m_storage;
    Galaxy* m_galaxy;

    HeaderBar m_header;
    BottomBar m_bottom;
    MessageView m_mview;
    ThreadView m_tview;
    SearchView m_sview;
    TextView m_textview;
    ChartView m_chartview;
    std::unique_ptr<GalaxyOpen> m_gopen;
    std::unique_ptr<GalaxyWarp> m_gwarp;

    std::string m_fn;
    int m_historyIdx;
};

#endif
