#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include <cstdint>
#include <vector>
#include <map>

#include <string>

#include "Points/points.hpp"
#include "lines.hpp"

class LineData {
public:
    LineData();
    void clearLinesAtTime(int const time);
    void addLineAtTime(int const time, std::vector<float> const& x, std::vector<float> const& y);
    void addLineAtTime(int const time, std::vector<Point2D<float>> const & line);

    void addPointToLine(int const time, int const line_id, const float x, const float y);

    void addPointToLineInterpolate(int const time, int const line_id, const float x, const float y);

    std::vector<int> getTimesWithLines() const;

    std::vector<Line2D> const& getLinesAtTime(int const time) const;
protected:

private:
    std::map<int,std::vector<Line2D>> _data;
    std::vector<Line2D> _empty;
};


#endif // LINE_DATA_HPP
