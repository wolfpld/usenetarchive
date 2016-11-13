#ifndef __MESSAGEVIEW_HPP__
#define __MESSAGEVIEW_HPP__

#include "View.hpp"

class MessageView : public View
{
public:
    MessageView();
    ~MessageView();

    void Resize();

    bool IsActive() const { return m_active; }
    void SetActive( bool active );

private:
    bool m_active;
    bool m_vertical;
};

#endif
