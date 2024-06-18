#ifndef POINT_DATA_HPP
#define POINT_DATA_HPP

#include <vector>
#include <map>
#include <string>

template<typename T>
struct Point2D {
    T x;
    T y;
};

class PointData {
public:
    PointData();
    void clearPointsAtTime(int const time);
    void addPointAtTime(int const time, float const x, float const y);

    std::vector<int> getTimesWithPoints() const;

    std::vector<Point2D<float>> const& getPointsAtTime(int const time) const;
protected:

private:
    std::map<int,std::vector<Point2D<float>>> _data;
    std::vector<Point2D<float>> _empty;

};

std::map<int,Point2D<float>> load_points_from_csv(std::string const& filename, int const frame_column, int const x_column, int const y_column);


#endif // POINT_DATA_HPP
