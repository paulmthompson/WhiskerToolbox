
#include "Line_Data.hpp"
#include "Points/points.hpp"
#include "utils/container.hpp"

#include <cmath>
#include <iostream>


void LineData::clearLinesAtTime(int const time) {
    if (!_lock_state.isLocked(time)) {
        _data[time].clear();

        notifyObservers();
    }
}

void LineData::clearLineAtTime(int const time, int const line_id) {
    if (!_lock_state.isLocked(time)) {
        if (line_id < _data[time].size()) {
            _data[time].erase(_data[time].begin() + line_id);
        }

        notifyObservers();
    }
}

void LineData::addLineAtTime(int const time, std::vector<float> const & x, std::vector<float> const & y) {
    if (!_lock_state.isLocked(time)) {
        auto new_line = create_line(x, y);
        _data[time].push_back(new_line);

        notifyObservers();
    }
}

void LineData::addLineAtTime(int const time, std::vector<Point2D<float>> const & line) {
    if (!_lock_state.isLocked(time)) {
        _data[time].push_back(line);

        notifyObservers();
    }
}

void LineData::addPointToLine(int const time, int const line_id, Point2D<float> point) {
    if (!_lock_state.isLocked(time)) {
        if (line_id < _data[time].size()) {
            _data[time][line_id].push_back(point);
        } else {
            std::cerr << "LineData::addPointToLine: line_id out of range" << std::endl;
            _data[time].emplace_back();
            _data[time].back().push_back(point);
        }

        notifyObservers();
    }
}

void LineData::addPointToLineInterpolate(int const time, int const line_id, Point2D<float> point) {
    if (!_lock_state.isLocked(time)) {
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

        notifyObservers();
    }
}

std::vector<Line2D> const & LineData::getLinesAtTime(int const time) const {
    // [] operator is not const because it inserts if mask is not present
    if (_data.find(time) != _data.end()) {
        return _data.at(time);
    } else {
        return _empty;
    }
}

std::vector<int> LineData::getTimesWithLines() const {
    std::vector<int> keys;
    keys.reserve(_data.size());
    for (auto const & kv: _data) {
        keys.push_back(kv.first);
    }
    return keys;
}
