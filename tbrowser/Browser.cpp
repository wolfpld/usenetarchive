#include "Browser.hpp"

Browser::Browser( std::unique_ptr<Archive>&& archive )
    : m_archive( std::move( archive ) )
    , m_header( m_archive->GetArchiveName().first, m_archive->GetShortDescription().second > 0 ? m_archive->GetShortDescription().first : nullptr )
    , m_tview( *m_archive )
{
}

Browser::~Browser()
{
}

void Browser::Entry()
{
    for(;;);
}
