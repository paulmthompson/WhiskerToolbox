
#include "TimeFrame.hpp"

TimeFrame::TimeFrame(std::vector<int> const & times)
{
    _times = times;
    _last_loaded_frame = 0;
    _total_frame_count = static_cast<int>(times.size());
}
