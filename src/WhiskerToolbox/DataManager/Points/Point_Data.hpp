#ifndef POINT_DATA_HPP
#define POINT_DATA_HPP


#include <map>
#include <string>
#include <vector>

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

    int getMaskHeight() const {return _mask_height;};
    int getMaskWidth() const {return _mask_width;};

    void setMaskHeight(int const height) {_mask_height = height;};
    void setMaskWidth(int const width) {_mask_width = width;};

    std::vector<Point2D<float>> const& getPointsAtTime(int const time) const;
protected:

private:
    std::map<int,std::vector<Point2D<float>>> _data;
    std::vector<Point2D<float>> _empty;

    int _mask_height {256};
    int _mask_width {256};

};

std::map<int,Point2D<float>> load_points_from_csv(std::string const& filename, int const frame_column, int const x_column, int const y_column);


#endif // POINT_DATA_HPP
