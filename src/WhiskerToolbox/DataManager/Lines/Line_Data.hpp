#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include <vector>
#include <map>

struct Point2D {
    float x;
    float y;
};

using Line2D = std::vector<Point2D>;

class LineData {
public:
    LineData();
    void addLineAtTime(const int time, const std::vector<float>& x, const std::vector<float>& y);

    std::vector<Line2D> getLinesAtTime(const int time);
protected:

private:
    std::map<int,std::vector<Line2D>> _data;

    Line2D _createLine(const std::vector<float>& x, const std::vector<float>& y);
};


#endif // LINE_DATA_HPP
