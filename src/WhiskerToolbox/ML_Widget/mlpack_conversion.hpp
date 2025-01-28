
#ifndef WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
#define WHISKERTOOLBOX_MLPACK_CONVERSION_HPP

#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include <mlpack/core.hpp>

#include <memory>

inline arma::Row<double> convertToMlpackArray(
        const std::shared_ptr<DigitalIntervalSeries>& series,
        std::size_t length)
{
    arma::Row<double> result(length, arma::fill::zeros);

    for (const auto& interval : series->getDigitalIntervalSeries()) {
        int start = static_cast<int>(interval.start);
        int end = static_cast<int>(interval.end);
        for (std::size_t i = start; i <= end && i < length; ++i) {
            result[i] = 1.0;
        }
    }

    return result;
}

inline void updateDigitalIntervalSeriesFromMlpackArray(
        const arma::Row<double>& array,
        std::shared_ptr<DigitalIntervalSeries> series)
{
    std::vector<std::pair<float, float>> intervals;
    bool in_interval = false;
    int start = 0;

    for (std::size_t i = 0; i < array.n_elem; ++i) {
        if (array[i] == 1.0 && !in_interval) {
            start = i;
            in_interval = true;
        } else if (array[i] == 0.0 && in_interval) {
            intervals.emplace_back(start, i - 1);
            in_interval = false;
        }
    }

    if (in_interval) {
        intervals.emplace_back(start, array.n_elem - 1);
    }

    series->setData(intervals);
}



inline arma::Mat<double> convertToMlpackMatrix(
        const std::shared_ptr<PointData>& pointData,
        std::size_t length)
{

    const auto& data = pointData->getData();
    size_t numRows = length;
    size_t numCols = 0;

    // Determine the maximum number of points at any time step
    for (const auto& [time, points] : data) {
        if (points.size() > numCols) {
            numCols = points.size();
        }
    }

    arma::Mat<double> result(numRows, numCols * 2, arma::fill::zeros);

    for (const auto& [time, points] : data) {
        for (size_t col = 0; col < points.size(); ++col) {
            result(time, col * 2) = points[col].x;
            result(time, col * 2 + 1) = points[col].y;
        }
    }

    return result;
}

inline void updatePointDataFromMlpackMatrix(
        const arma::Mat<double>& matrix,
        std::shared_ptr<PointData> pointData)
{

    for (std::size_t row = 0; row < matrix.n_rows; ++row) {
        std::vector<Point2D<float>> points;
        for (std::size_t col = 0; col < matrix.n_cols; col += 2) {
            if (matrix(row, col) != 0.0 || matrix(row, col + 1) != 0.0) {
                points.emplace_back(Point2D<float>{
                        static_cast<float>(matrix(row, col)),
                        static_cast<float>(matrix(row, col + 1))
                });
            }
        }
        pointData->addPointsAtTime(row, points); // need to find time at row
    }

}


#endif //WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
