#ifndef __MESSAGEVIEW_HPP__
#define __MESSAGEVIEW_HPP__

#include <vector>

#include "View.hpp"

class Archive;
class PersistentStorage;

class MessageView : public View
{
public:
    MessageView( Archive& archive, PersistentStorage& storage );

    void Resize();
    bool Display( uint32_t idx, int move );
    void Close();
    void SwitchHeaders();
    void SwitchROT13();

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
    void PrintRot13( const char* start, const char* end );

    std::vector<Line> m_lines;
    Archive& m_archive;
    PersistentStorage& m_storage;
    const char* m_text;
    int32_t m_idx;
    int m_top;
    bool m_active;
    bool m_vertical;
    bool m_allHeaders;
    bool m_rot13;
};

#endif
