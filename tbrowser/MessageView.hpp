#ifndef __MESSAGEVIEW_HPP__
#define __MESSAGEVIEW_HPP__

#include "View.hpp"

class Archive;

class MessageView : public View
{
public:
    MessageView( Archive& archive );
    ~MessageView();

    void Resize();
    void Display( uint32_t idx );
    void Close();

    bool IsActive() const { return m_active; }
    uint32_t DisplayedMessage() const { return m_idx; }

private:
    void Draw();

    Archive& m_archive;
    const char* m_text;
    const char* m_top;
    int32_t m_idx;
    bool m_active;
    bool m_vertical;
};

#endif
