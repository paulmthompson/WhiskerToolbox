#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include <vector>
#include <map>
#include <string>

#include "Points/Point_Data.hpp"

#if defined _WIN32 || defined __CYGWIN__
#define DLLOPT __declspec(dllexport)
//#define DLLOPT __declspec(dllexport)
#else
#define DLLOPT __attribute__((visibility("default")))
#endif

using Line2D = std::vector<Point2D>;

class DLLOPT LineData {
public:
    LineData();
    void clearLinesAtTime(const int time);
    void addLineAtTime(const int time, const std::vector<float>& x, const std::vector<float>& y);
    void addLineAtTime(const int time, const std::vector<Point2D> line);

    std::vector<Line2D> getLinesAtTime(const int time);
protected:

private:
    std::map<int,std::vector<Line2D>> _data;

    Line2D _createLine(const std::vector<float>& x, const std::vector<float>& y);
};

void save_line_as_csv(Line2D const& line, std::string const& filename, int const point_precision = 2);

#endif // LINE_DATA_HPP
