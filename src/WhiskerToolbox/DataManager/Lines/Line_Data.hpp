#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include <cstdint>
#include <vector>
#include <map>

#include <string>

#include "Points/points.hpp"
#include "lines.hpp"
#include "LockState/LockState.hpp"

class LineData {
public:
    LineData();
    LineData(std::map<int,std::vector<Line2D>> const& data) : _data(data) {};
    void clearLinesAtTime(int const time);
    void addLineAtTime(int const time, std::vector<float> const& x, std::vector<float> const& y);
    void addLineAtTime(int const time, std::vector<Point2D<float>> const & line);

    void addPointToLine(int const time, int const line_id, const float x, const float y);

    void addPointToLineInterpolate(int const time, int const line_id, const float x, const float y);

    std::vector<int> getTimesWithLines() const;

    std::vector<Line2D> const& getLinesAtTime(int const time) const;

    std::map<int, std::vector<Line2D>> const& getData() const {return _data;};

    void lockTime(int time) { _lock_state.lock(time); }
    void unlockTime(int time) { _lock_state.unlock(time); }
    bool isTimeLocked(int time) const { return _lock_state.isLocked(time); }

    void lockUntil(int time)
    {
        _lock_state.clear();
        for (int i = 0; i <= time; i++) {
            _lock_state.lock(i);
        }
    }
protected:

private:
    std::map<int,std::vector<Line2D>> _data;
    std::vector<Line2D> _empty;
    LockState _lock_state;
};


#endif // LINE_DATA_HPP
