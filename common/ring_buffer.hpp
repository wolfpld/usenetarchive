#ifndef __DARKRL__RINGBUFFER_HPP__
#define __DARKRL__RINGBUFFER_HPP__

#include <cassert>
#include <cstddef>
#include <memory>
#include <stdexcept>

template<typename T, typename A = std::allocator<T>>
class ring_buffer
{
public:
    typedef T value_type;
    typedef A allocator_type;
    typedef typename allocator_type::size_type size_type;
    typedef typename allocator_type::difference_type difference_type;
    typedef typename allocator_type::reference reference;
    typedef typename allocator_type::const_reference const_reference;
    typedef typename allocator_type::pointer pointer;
    typedef typename allocator_type::const_pointer const_pointer;
    typedef ring_buffer<T, A> class_type;
    class iterator;
    class const_iterator;
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    explicit ring_buffer( size_type size, const allocator_type& a = allocator_type() );
    ~ring_buffer();
    allocator_type get_allocator() const;

    size_type size() const;
    size_type max_size() const;
    bool empty() const;

    size_type capacity() const;

    reference front();
    const_reference front() const;
    reference back();
    const_reference back() const;

    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;
    iterator end();
    const_iterator end() const;
    const_iterator cend() const;
    reverse_iterator rbegin() { return reverse_iterator( end() ); }
    const_reverse_iterator rbegin() const { return const_reverse_iterator( end() ); }
    const_reverse_iterator crbegin() const { return const_reverse_iterator( end() ); }
    reverse_iterator rend() { return reverse_iterator( begin() ); }
    const_reverse_iterator rend() const { return const_reverse_iterator( begin() ); }
    const_reverse_iterator crend() const { return const_reverse_iterator( begin() ); }

    bool push_back( const value_type& );
    void pop_front();
    void clear();

    reference operator[]( size_type n );
    const_reference operator[]( size_type n ) const;
    reference at( size_type n );
    const_reference at( size_type n ) const;

    ring_buffer( const class_type& ) = delete;
    ring_buffer( class_type&& ) = delete;
    class_type& operator=( const class_type& ) = delete;
    class_type& operator=( class_type&& ) = delete;

private:
    pointer wrap_fwd( pointer ptr ) const;
    pointer wrap_bck( pointer ptr ) const;

    const size_type m_capacity;
    allocator_type m_allocator;
    pointer m_buffer;
    pointer m_front;
    pointer m_back;
};

template<typename T, typename A>
class ring_buffer<T, A>::iterator : public std::iterator<std::random_access_iterator_tag, value_type, size_type, pointer, reference>
{
public:
    typedef ring_buffer<T, A> parent_type;
    typedef typename parent_type::iterator self_type;

    iterator( parent_type& parent, size_type index )
        : parent( parent )
        , index( index )
    {
    }

    self_type& operator++()
    {
        ++index;
        return *this;
    }

    self_type operator++( int )
    {
        self_type old( *this );
        operator++();
        return old;
    }

    self_type& operator--()
    {
        --index;
        return *this;
    }

    self_type operator--( int )
    {
        self_type old( *this );
        operator--();
        return old;
    }

    reference operator*() { return parent[index]; }
    pointer operator->() { return &(parent[index]); }

    bool operator==( const self_type& other ) const
    {
        return &parent == &other.parent && index == other.index;
    }

    bool operator!=( const self_type& other ) const
    {
        return !( other == *this );
    }

private:
    parent_type& parent;
    size_type index;
};

template<typename T, typename A>
class ring_buffer<T, A>::const_iterator : public std::iterator<std::random_access_iterator_tag, value_type, size_type, pointer, reference>
{
public:
    typedef const ring_buffer<T, A> parent_type;
    typedef typename parent_type::const_iterator self_type;

    const_iterator( parent_type& parent, size_type index )
        : parent( parent )
        , index( index )
    {
    }

    self_type& operator++()
    {
        ++index;
        return *this;
    }

    self_type operator++( int )
    {
        self_type old( *this );
        operator++();
        return old;
    }

    self_type& operator--()
    {
        --index;
        return *this;
    }

    self_type operator--( int )
    {
        self_type old( *this );
        operator--();
        return old;
    }

    const_reference operator*() { return parent[index]; }
    const_pointer operator->() { return &(parent[index]); }

    bool operator==( const self_type& other ) const
    {
        return &parent == &other.parent && index == other.index;
    }

    bool operator!=( const self_type& other ) const
    {
        return !( other == *this );
    }

private:
    parent_type& parent;
    size_type index;
};


template<typename T, typename A>
inline ring_buffer<T, A>::ring_buffer( size_type capacity, const allocator_type& allocator )
    : m_capacity( capacity )
    , m_allocator( allocator )
    , m_buffer( m_allocator.allocate( capacity ) )
    , m_front( nullptr )
    , m_back( m_buffer )
{
    assert( capacity > 0 );
}

template<typename T, typename A>
inline ring_buffer<T, A>::~ring_buffer()
{
    clear();
    m_allocator.deallocate( m_buffer, m_capacity );
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::allocator_type ring_buffer<T, A>::get_allocator() const
{
    return m_allocator;
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::size_type ring_buffer<T, A>::size() const
{
    return !m_front ? 0 : ( m_back > m_front ? m_back : m_back + m_capacity ) - m_front;
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::size_type ring_buffer<T, A>::max_size() const
{
    return m_allocator.max_size();
}

template<typename T, typename A>
inline bool ring_buffer<T, A>::empty() const
{
    return !m_front;
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::size_type ring_buffer<T, A>::capacity() const
{
    return m_capacity;
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::reference ring_buffer<T, A>::front()
{
    assert( m_front );
    return *m_front;
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::const_reference ring_buffer<T, A>::front() const
{
    assert( m_front );
    return *m_front;
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::reference ring_buffer<T, A>::back()
{
    assert( m_front );
    return *wrap_bck( m_back-1 );
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::const_reference ring_buffer<T, A>::back() const
{
    assert( m_front );
    return *wrap_bck( m_back-1 );
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::iterator ring_buffer<T, A>::begin()
{
    return iterator( *this, 0 );
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::const_iterator ring_buffer<T, A>::begin() const
{
    return const_iterator( *this, 0 );
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::const_iterator ring_buffer<T, A>::cbegin() const
{
    return const_iterator( *this, 0 );
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::iterator ring_buffer<T, A>::end()
{
    return iterator( *this, size() );
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::const_iterator ring_buffer<T, A>::end() const
{
    return const_iterator( *this, size() );
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::const_iterator ring_buffer<T, A>::cend() const
{
    return const_iterator( *this, size() );
}

template<typename T, typename A>
inline bool ring_buffer<T, A>::push_back( const value_type& value )
{
    if( m_front && m_front == m_back )
    {
        m_allocator.destroy( m_back );
    }

    m_allocator.construct( m_back, value );

    value_type* const next = wrap_fwd( m_back+1 );
    if( !m_front )
    {
        m_front = m_back;
        m_back = next;
        return true;
    }
    else if( m_front == m_back )
    {
        m_front = m_back = next;
        return false;
    }
    else
    {
        m_back = next;
        return true;
    }
}

template<typename T, typename A>
inline void ring_buffer<T, A>::pop_front()
{
    assert( m_front );
    m_allocator.destroy( m_front );
    value_type* const next = wrap_fwd( m_front+1 );
    if( next == m_back )
    {
        m_front = nullptr;
    }
    else
    {
        m_front = next;
    }
}

template<typename T, typename A>
inline void ring_buffer<T, A>::clear()
{
    if( m_front )
    {
        do
        {
            m_allocator.destroy( m_front );
            m_front = wrap_fwd( m_front+1 );
        }
        while( m_front != m_back );
        m_front = nullptr;
    }
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::reference ring_buffer<T, A>::operator[]( size_type n )
{
    return *wrap_fwd( m_front + n );
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::const_reference ring_buffer<T, A>::operator[]( size_type n ) const
{
    return *wrap_fwd( m_front + n );
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::reference ring_buffer<T, A>::at( size_type n )
{
    if( n > size() ) throw std::out_of_range( "ring_buffer" );
    return (*this)[n];
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::const_reference ring_buffer<T, A>::at( size_type n ) const
{
    if( n > size() ) throw std::out_of_range( "ring_buffer" );
    return (*this)[n];
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::pointer ring_buffer<T, A>::wrap_fwd( pointer ptr ) const
{
    assert( ptr >= m_buffer );
    assert( ptr < m_buffer + m_capacity * 2 );
    if( ptr >= m_buffer + m_capacity )
    {
        return ptr - m_capacity;
    }
    else
    {
        return ptr;
    }
}

template<typename T, typename A>
inline typename ring_buffer<T, A>::pointer ring_buffer<T, A>::wrap_bck( pointer ptr ) const
{
    assert( ptr < m_buffer + m_capacity );
    assert( ptr > m_buffer - m_capacity );
    if( ptr < m_buffer )
    {
        return ptr + m_capacity;
    }
    else
    {
        return ptr;
    }
}

#endif
