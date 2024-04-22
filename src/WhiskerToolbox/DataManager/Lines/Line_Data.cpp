
#include "Line_Data.hpp"

LineData::LineData()
{

}

void LineData::clearLinesAtTime(const int time)
{
    _data[time].clear();
}

void LineData::addLineAtTime(const int time, const std::vector<float>& x, const std::vector<float>& y)
{
    auto new_line = _createLine(x,y);

    _data[time].push_back(new_line);
}

std::vector<Line2D> LineData::getLinesAtTime(const int time)
{
    return _data[time];
}

Line2D LineData::_createLine(const std::vector<float>& x, const std::vector<float>& y)
{
    auto new_line = Line2D{x.size()};

    for (int i = 0; i < x.size(); i++)
    {
        new_line[i] = Point2D{x[i],y[i]};
    }

    return new_line;
}
