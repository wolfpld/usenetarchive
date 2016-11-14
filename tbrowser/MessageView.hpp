#ifndef __MESSAGEVIEW_HPP__
#define __MESSAGEVIEW_HPP__

#include <vector>

#include "View.hpp"

class Archive;

class MessageView : public View
{
public:
    MessageView( Archive& archive );
    ~MessageView();

    void Resize();
    bool Display( uint32_t idx, int move );
    void Close();
    void SwitchHeaders();

    bool IsActive() const { return m_active; }
    uint32_t DisplayedMessage() const { return m_idx; }

private:
    enum
    {
        L_Header = 0,
        L_Quote0 = 1,
        L_Quote1 = 2,
        L_Quote2 = 3,
        L_Quote3 = 4,
        L_Quote4 = 5,
        L_Signature = 6
    };
    struct Line
    {
        uint32_t offset : 24;
        uint32_t flags  : 8;
    };

    void Draw();
    void PrepareLines();

    std::vector<Line> m_lines;
    Archive& m_archive;
    const char* m_text;
    int32_t m_idx;
    int m_top;
    bool m_active;
    bool m_vertical;
    bool m_allHeaders;
};

#endif
