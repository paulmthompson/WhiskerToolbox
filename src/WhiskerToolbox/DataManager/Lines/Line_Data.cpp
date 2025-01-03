
#include "Line_Data.hpp"
#include "utils/container.hpp"

#include <cmath>

#include <iostream>


LineData::LineData()
{

}

void LineData::clearLinesAtTime(int const time)
{
    if (!_lock_state.isLocked(time)) {
        _data[time].clear();
    }
}

void LineData::addLineAtTime(int const time, std::vector<float> const& x, std::vector<float> const& y)
{
    if (!_lock_state.isLocked(time)) {
        auto new_line = create_line(x, y);
        _data[time].push_back(new_line);
    }
}

void LineData::addLineAtTime(int const time, std::vector<Point2D<float>> const & line)
{
    if (!_lock_state.isLocked(time)) {
        _data[time].push_back(std::move(line));
    }
}

void LineData::addPointToLine(int const time, int const line_id, const float x, const float y)
{
    if (!_lock_state.isLocked(time)) {
        if (line_id < _data[time].size()) {
            _data[time][line_id].push_back(Point2D<float>{x, y});
        } else {
            std::cerr << "LineData::addPointToLine: line_id out of range" << std::endl;
            _data[time].push_back(Line2D{});
            _data[time].back().push_back(Point2D<float>{x, y});
        }
    }
}

void LineData::addPointToLineInterpolate(int const time, int const line_id, const float x, const float y)
{
    if (!_lock_state.isLocked(time)) {
        if (line_id >= _data[time].size()) {
            std::cerr << "LineData::addPointToLineInterpolate: line_id out of range" << std::endl;
            _data[time].push_back(Line2D{});
        }

        Line2D& line = _data[time][line_id];
        Point2D<float> new_point{x, y};
        if (!line.empty()) {
            Point2D<float> last_point = line.back();
            float distance = std::sqrt(std::pow(new_point.x - last_point.x, 2) + std::pow(new_point.y - last_point.y, 2));
            int n = static_cast<int>(distance / 2.0f);
            for (int i = 1; i <= n; ++i) {
                float t = static_cast<float>(i) / (n + 1);
                float interp_x = last_point.x + t * (new_point.x - last_point.x);
                float interp_y = last_point.y + t * (new_point.y - last_point.y);
                line.push_back(Point2D<float>{interp_x, interp_y});
            }
        }
        line.push_back(new_point);
        smooth_line(line);
    }
}

std::vector<Line2D> const& LineData::getLinesAtTime(int const time) const
{
    // [] operator is not const because it inserts if mask is not present
    if (_data.find(time) != _data.end())
    {
        return _data.at(time);
    } else {
        return _empty;
    }
}

std::vector<int> LineData::getTimesWithLines() const
{
    std::vector<int> keys;
    keys.reserve(_data.size());
    for (auto kv : _data) {
        keys.push_back(kv.first);
    }
    return keys;
}
