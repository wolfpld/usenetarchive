#include "Browser.hpp"

Browser::Browser( std::unique_ptr<Archive>&& archive )
    : m_archive( std::move( archive ) )
    , m_header( m_archive->GetArchiveName().first, m_archive->GetShortDescription().second > 0 ? m_archive->GetShortDescription().first : nullptr )
    , m_bottom( m_archive->NumberOfTopLevel() )
    , m_tview( *m_archive )
{
    m_bottom.Update( 0 );
}

Browser::~Browser()
{
}

void Browser::Entry()
{
    while( m_tview.GetKey() );
}
