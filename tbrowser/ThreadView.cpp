#include "../libuat/Archive.hpp"

#include "ThreadView.hpp"

ThreadView::ThreadView( const Archive& archive )
    : View( 0, 1, -1, -1 )
    , m_data( archive.NumberOfMessages() )
{
    unsigned int idx = 0;
    const auto toplevel = archive.GetTopLevel();
    for( int i=0; i<toplevel.size; i++ )
    {
        m_data[idx].msgid = toplevel.ptr[i];
        m_data[idx].valid = 1;
        idx += archive.GetTotalChildrenCount( toplevel.ptr[i] );
    }
}

ThreadView::~ThreadView()
{
}
