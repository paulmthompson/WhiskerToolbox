#include "TimeScrollBar.hpp"

int TimeScrollBar::_getSnapFrame(int current_frame) {
    // No video support compiled in, so no snapping available
    // Just return the current frame unchanged
    return current_frame;
}
