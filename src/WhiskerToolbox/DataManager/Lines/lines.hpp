#ifndef DATAMANAGER_LINES_HPP
#define DATAMANAGER_LINES_HPP

#include "CoreGeometry/points.hpp"

#include <cstdint>
#include <vector>

class Line2D {
public:
    Line2D() = default;
    Line2D(std::vector<Point2D<float>> points) : points_(std::move(points)) {}

    size_t size() const {
        return points_.size();
    }

    Point2D<float> front() const {
        return points_.front();
    }

    bool empty() const {
        return points_.empty();
    }

    void push_back(Point2D<float> const & point) {
        points_.push_back(point);
    }

    Point2D<float> back() const {
        return points_.back();
    }
    Point2D<float> operator[](size_t index) const {
        return points_[index];
    }

    std::vector<Point2D<float>>::iterator begin() {
        return points_.begin();
    }

    std::vector<Point2D<float>>::iterator end() {
        return points_.end();
    }
    std::vector<Point2D<float>>::const_iterator begin() const {
        return points_.begin();
    }
    std::vector<Point2D<float>>::const_iterator end() const {
        return points_.end();
    }

    void erase(std::vector<Point2D<float>>::iterator it) {
        points_.erase(it);
    }

    void erase(std::vector<Point2D<float>>::iterator first, std::vector<Point2D<float>>::iterator last) {
        points_.erase(first, last);
    }

private:
    std::vector<Point2D<float>> points_; 

};


Line2D create_line(std::vector<float> const & x, std::vector<float> const & y);


void smooth_line(Line2D & line);

std::vector<uint8_t> line_to_image(Line2D & line, int height, int width);



#endif// DATAMANAGER_LINES_HPP
