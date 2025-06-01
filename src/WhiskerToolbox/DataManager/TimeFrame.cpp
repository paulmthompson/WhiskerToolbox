
#include "TimeFrame.hpp"

TimeFrame::TimeFrame(std::vector<int> const & times)
{
    _times = times;
    _total_frame_count = static_cast<int>(times.size());
}

float getTimeIndexForSeries(int64_t time_value_in_source_frame,
                            TimeFrame const & source_time_frame,
                            TimeFrame const & destination_time_frame) {
    // Check if the source frame of the time value is the same object instance as the series' data frame.
    // This comparison by address assumes that if they are conceptually the "same" frame,
    // they would be represented by the same object instance in this function's context.
    if (&source_time_frame == &destination_time_frame) {
        // Frames are the same. The time value can be used directly.
        return time_value_in_source_frame;
    } else {
        // Frames are different.
        // The `time_value_in_source_frame` is a time in the `source_value_frame`.
        // We need to find the corresponding index in the `destination_frame`
        // that is closest to this absolute time value.
        auto index_in_series_frame = destination_time_frame.getIndexAtTime(time_value_in_source_frame);
        return index_in_series_frame;
    }
}
