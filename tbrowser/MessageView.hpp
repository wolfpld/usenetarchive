#ifndef __MESSAGEVIEW_HPP__
#define __MESSAGEVIEW_HPP__

#include <vector>

#include "View.hpp"

#include "../common/ExpandingBuffer.hpp"
#include "../common/MessageLines.hpp"

class Archive;
class PersistentStorage;

enum class ViewSplit
{
    Auto,
    Vertical,
    Horizontal,

    NUM_VIEW_SPLIT
};

class MessageView : public View
{
public:
    MessageView( Archive& archive, PersistentStorage& storage );

    void Reset( Archive& archive );

    void Draw();
    void Resize();
    bool Display( uint32_t idx, int move );
    void Close();
    void SwitchHeaders();
    void SwitchROT13();

    bool IsActive() const { return m_active; }
    uint32_t DisplayedMessage() const { return m_idx; }

    ViewSplit NextViewSplit();
    ViewSplit GetViewSplit() const { return m_viewSplit; }

private:
    void PrepareLines();
    void PrintRot13( const char* start, const char* end );

    ExpandingBuffer m_eb;
    Archive* m_archive;
    PersistentStorage& m_storage;
    MessageLines m_lines;
    const char* m_text;
    int32_t m_idx;
    int m_top;
    int m_linesWidth;
    bool m_active;
    bool m_vertical;
    bool m_allHeaders;
    bool m_rot13;
    ViewSplit m_viewSplit;
};

#endif
