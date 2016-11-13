#ifndef __MESSAGEVIEW_HPP__
#define __MESSAGEVIEW_HPP__

#include "View.hpp"

class Archive;

class MessageView : public View
{
public:
    MessageView( const Archive& archive );
    ~MessageView();

    void Resize();

    bool IsActive() const { return m_active; }
    void SetActive( bool active );

private:
    const Archive& m_archive;

    bool m_active;
    bool m_vertical;
};

#endif
