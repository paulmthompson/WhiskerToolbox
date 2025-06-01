
#include "TimeFrame.hpp"

TimeFrame::TimeFrame(std::vector<int> const & times)
{
    _times = times;
    _total_frame_count = static_cast<int>(times.size());
}
