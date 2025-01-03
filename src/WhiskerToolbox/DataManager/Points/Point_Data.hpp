#ifndef POINT_DATA_HPP
#define POINT_DATA_HPP

#include "points.hpp"

#include <map>
#include <string>
#include <vector>


class PointData {
public:
    PointData();
    PointData(std::map<int, Point2D<float>> data);
    PointData(std::map<int,std::vector<Point2D<float>>> data);
    void clearPointsAtTime(int const time);
    void addPointAtTime(int const time, float const x, float const y);

    std::vector<int> getTimesWithPoints() const;

    int getMaskHeight() const {return _mask_height;};
    int getMaskWidth() const {return _mask_width;};

    void setMaskHeight(int const height) {_mask_height = height;};
    void setMaskWidth(int const width) {_mask_width = width;};

    std::vector<Point2D<float>> const& getPointsAtTime(int const time) const;

    std::map<int, std::vector<Point2D<float>>> const& getData() const {return _data;};
protected:

private:
    std::map<int,std::vector<Point2D<float>>> _data;
    std::vector<Point2D<float>> _empty;

    int _mask_height {256};
    int _mask_width {256};

};

std::map<int,Point2D<float>> load_points_from_csv(
    std::string const& filename,
    int const frame_column,
    int const x_column,
    int const y_column,
    char const column_delim = ' ');

std::map<std::string, std::map<int, Point2D<float>>> load_multiple_points_from_csv(std::string const& filename, int const frame_column);

#endif // POINT_DATA_HPP
