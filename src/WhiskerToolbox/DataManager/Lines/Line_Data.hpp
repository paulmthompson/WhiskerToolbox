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
    void clearLinesAtTime(int const time);
    void addLineAtTime(int const time, std::vector<float> const& x, std::vector<float> const& y);
    void addLineAtTime(int const time, std::vector<Point2D> const line);

    std::vector<Line2D> const& getLinesAtTime(int const time) const;
protected:

private:
    std::map<int,std::vector<Line2D>> _data;
    std::vector<Line2D> _empty;

    Line2D _createLine(std::vector<float> const& x, std::vector<float> const& y);
};

void save_line_as_csv(Line2D const& line, std::string const& filename, int const point_precision = 2);

Line2D load_line_from_csv(std::string const& filename);

#endif // LINE_DATA_HPP
