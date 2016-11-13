#include "MessageView.hpp"

MessageView::MessageView()
    : View( 0, 0, 1, 1 )
    , m_active( false )
{
}

MessageView::~MessageView()
{
}

void MessageView::Resize()
{
    if( !m_active ) return;
}

void MessageView::SetActive( bool active )
{
    if( m_active != active )
    {

    }
    m_active = active;
}
