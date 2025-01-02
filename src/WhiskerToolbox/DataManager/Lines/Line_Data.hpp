#ifndef LINE_DATA_HPP
#define LINE_DATA_HPP

#include <cstdint>
#include <vector>
#include <map>
#include <string>

#include "Points/Point_Data.hpp"

using Line2D = std::vector<Point2D<float>>;

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

void save_line_as_csv(Line2D const& line, std::string const& filename, int const point_precision = 2);

Line2D load_line_from_csv(std::string const& filename);

Line2D create_line(std::vector<float> const& x, std::vector<float> const& y);

void smooth_line(Line2D& line);

std::vector<uint8_t> line_to_image(Line2D& line, int height, int width);

#endif // LINE_DATA_HPP
