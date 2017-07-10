#include "ThreadTree.hpp"

ThreadTree::ThreadTree( size_t size )
    : m_data( size )
    , m_tree( size )
{
}

void ThreadTree::Reset( size_t size )
{
    m_data = std::vector<ThreadData>( size );
    m_tree = std::vector<BitSet>( size );
}
