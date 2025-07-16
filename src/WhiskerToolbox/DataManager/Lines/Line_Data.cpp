#include "Line_Data.hpp"

#include "CoreGeometry/points.hpp"
#include "utils/map_timeseries.hpp"

#include <cmath>
#include <iostream>
#include <ranges>

// ========== Constructors ==========

LineData::LineData(std::map<TimeFrameIndex, std::vector<Line2D>> const & data)
    : _data(data)
{

}

// ========== Setters ==========

bool LineData::clearAtTime(TimeFrameIndex const time, bool notify) {

    if (clear_at_time(time, _data)) {
        if (notify) {
            notifyObservers();
        }
        return true;
    }
    return false;   
}

bool LineData::clearAtTime(TimeFrameIndex const time, int const line_id, bool notify) {

    if (clear_at_time(time, line_id, _data)) {
        if (notify) {
            notifyObservers();
        }
        return true;
    }
    return false;
}

void LineData::addAtTime(TimeFrameIndex const time, std::vector<float> const & x, std::vector<float> const & y, bool notify) {

    auto new_line = create_line(x, y);
    add_at_time(time, new_line, _data);

    if (notify) {
        notifyObservers();
    }
}

void LineData::addAtTime(TimeFrameIndex const time, std::vector<Point2D<float>> const & line, bool notify) {
    addAtTime(time, std::move(Line2D(line)), notify);
}

void LineData::addAtTime(TimeFrameIndex const time, Line2D const & line, bool notify) {
    add_at_time(time, line, _data);

    if (notify) {
        notifyObservers();
    }
}

void LineData::addPointToLine(TimeFrameIndex const time, int const line_id, Point2D<float> point, bool notify) {

    if (line_id < _data[time].size()) {
        _data[time][line_id].push_back(point);
    } else {
        std::cerr << "LineData::addPointToLine: line_id out of range" << std::endl;
        _data[time].emplace_back();
        _data[time].back().push_back(point);
    }

    if (notify) {
        notifyObservers();
    }
}

void LineData::addPointToLineInterpolate(TimeFrameIndex const time, int const line_id, Point2D<float> point, bool notify) {

    if (line_id >= _data[time].size()) {
        std::cerr << "LineData::addPointToLineInterpolate: line_id out of range" << std::endl;
        _data[time].emplace_back();
    }

    Line2D & line = _data[time][line_id];
    if (!line.empty()) {
        Point2D<float> const last_point = line.back();
        double const distance = std::sqrt(std::pow(point.x - last_point.x, 2) + std::pow(point.y - last_point.y, 2));
        int const n = static_cast<int>(distance / 2.0f);
        for (int i = 1; i <= n; ++i) {
            float const t = static_cast<float>(i) / static_cast<float>(n + 1);
            float const interp_x = last_point.x + t * (point.x - last_point.x);
            float const interp_y = last_point.y + t * (point.y - last_point.y);
            line.push_back(Point2D<float>{interp_x, interp_y});
        }
    }
    line.push_back(point);
    smooth_line(line);

    if (notify) {
        notifyObservers();
    }
}

// ========== Getters ==========

std::vector<Line2D> const & LineData::getAtTime(TimeFrameIndex const time) const {
    return get_at_time(time, _data, _empty);
}

std::vector<Line2D> const & LineData::getAtTime(TimeFrameIndex const time, 
                                                TimeFrame const * source_timeframe,
                                                TimeFrame const * line_timeframe) const {

    return get_at_time(time, _data, _empty, source_timeframe, line_timeframe);
}

// ========== Image Size ==========

void LineData::changeImageSize(ImageSize const & image_size)
{
    if (_image_size.width == -1 || _image_size.height == -1) {
        std::cout << "No size set for current image. "
                  << " Please set a valid image size before trying to scale" << std::endl;
    }

    if (_image_size.width == image_size.width && _image_size.height == image_size.height) {
        std::cout << "Image size is the same. No need to scale" << std::endl;
        return;
    }

    float const scale_x = static_cast<float>(image_size.width) / static_cast<float>(_image_size.width);
    float const scale_y = static_cast<float>(image_size.height) / static_cast<float>(_image_size.height);

    for (auto & [time, lines] : _data) {
        for (auto & line : lines) {
            for (auto & point : line) {
                point.x *= scale_x;
                point.y *= scale_y;
            }
        }
    }
    _image_size = image_size;

}

// ========== Copy and Move ==========

std::size_t LineData::copyTo(LineData& target, TimeFrameInterval const & interval, bool notify) const {
    if (interval.start > interval.end) {
        std::cerr << "LineData::copyTo: interval start (" << interval.start.getValue() 
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_lines_copied = 0;

    // Iterate through all times in the source data within the interval
    for (auto const & [time, lines] : _data) {
        if (time >= interval.start && time <= interval.end && !lines.empty()) {
            for (auto const& line : lines) {
                target.addAtTime(time, line, false); // Don't notify for each operation
                total_lines_copied++;
            }
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_lines_copied > 0) {
        target.notifyObservers();
    }

    return total_lines_copied;
}

std::size_t LineData::copyTo(LineData& target, std::vector<TimeFrameIndex> const& times, bool notify) const {
    std::size_t total_lines_copied = 0;

    // Copy lines for each specified time
    for (TimeFrameIndex time : times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            for (auto const& line : it->second) {
                target.addAtTime(time, line, false); // Don't notify for each operation
                total_lines_copied++;
            }
        }
    }

    // Notify observer only once at the end if requested
    if (notify && total_lines_copied > 0) {
        target.notifyObservers();
    }

    return total_lines_copied;
}

std::size_t LineData::moveTo(LineData& target, TimeFrameInterval const & interval, bool notify) {
    if (interval.start > interval.end) {
        std::cerr << "LineData::moveTo: interval start (" << interval.start.getValue() 
                  << ") must be <= interval end (" << interval.end.getValue() << ")" << std::endl;
        return 0;
    }

    std::size_t total_lines_moved = 0;
    std::vector<TimeFrameIndex> times_to_clear;

    // First, copy all lines in the interval to target
    for (auto const & [time, lines] : _data) {
        if (time >= interval.start && time <= interval.end && !lines.empty()) {
            for (auto const& line : lines) {
                target.addAtTime(time, line, false); // Don't notify for each operation
                total_lines_moved++;
            }
            times_to_clear.push_back(time);
        }
    }

    // Then, clear all the times from source
    for (TimeFrameIndex time : times_to_clear) {
        clearAtTime(time, false); // Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_lines_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_lines_moved;
}

std::size_t LineData::moveTo(LineData& target, std::vector<TimeFrameIndex> const & times, bool notify) {
    std::size_t total_lines_moved = 0;
    std::vector<TimeFrameIndex> times_to_clear;

    // First, copy lines for each specified time to target
    for (TimeFrameIndex time : times) {
        auto it = _data.find(time);
        if (it != _data.end() && !it->second.empty()) {
            for (auto const& line : it->second) {
                target.addAtTime(time, line, false); // Don't notify for each operation
                total_lines_moved++;
            }
            times_to_clear.push_back(time);
        }
    }

    // Then, clear all the times from source
    for (TimeFrameIndex time : times_to_clear) {
        clearAtTime(time, false); // Don't notify for each operation
    }

    // Notify observers only once at the end if requested
    if (notify && total_lines_moved > 0) {
        target.notifyObservers();
        notifyObservers();
    }

    return total_lines_moved;
}