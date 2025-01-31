
#ifndef WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
#define WHISKERTOOLBOX_MLPACK_CONVERSION_HPP

#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "DataManager/Points/Point_Data.hpp"

#include <mlpack/core.hpp>

#include <memory>

////////////////////////////////////////////////////////

// DigitalIntervalSeries

/**
 * Convert a DigitalIntervalSeries to an mlpack row vector
 * @param series The DigitalIntervalSeries to convert
 * @param timestamps The timestamps to convert
 * @return arma::Row<double> The mlpack row vector
 */
inline arma::Row<double> convertToMlpackArray(
        const std::shared_ptr<DigitalIntervalSeries>& series,
        std::vector<std::size_t> timestamps)
{

    auto length = timestamps.size();
    arma::Row<double> result(length, arma::fill::zeros);

    for (std::size_t i = 0; i < length; ++i) {
        if (series->isEventAtTime(timestamps[i])) {
            result[i] = 1.0;
        }
    }

    return result;
}

/**
 * Update a DigitalIntervalSeries from an mlpack row vector
 * @param array The mlpack row vector
 * @param timestamps The timestamps to update
 * @param series The DigitalIntervalSeries to update
 * @param threshold The threshold to use for the update
 */
inline void updateDigitalIntervalSeriesFromMlpackArray(
        const arma::Row<double>& array,
        std::vector<std::size_t> timestamps,
        std::shared_ptr<DigitalIntervalSeries> series,
        float threshold = 0.5)
{
    //convert array to vector of bool based on some threshold

    std::vector<bool> events;
    for (std::size_t i = 0; i < array.n_elem; ++i) {
        events.push_back(array[i] > threshold);
    }

    series->setEventsAtTimes(timestamps, events);
}

////////////////////////////////////////////////////////

// PointData

/**
 * Convert a PointData to an mlpack matrix
 * @param pointData The PointData to convert
 * @param timestamps The timestamps to convert
 * @return arma::Mat<double> The mlpack matrix
 */
inline arma::Mat<double> convertToMlpackMatrix(
        const std::shared_ptr<PointData>& pointData,
        std::vector<std::size_t> timestamps)
{

    const size_t numRows = timestamps.size();
    const size_t numCols = pointData->getMaxPoints();

    arma::Mat<double> result(numRows, numCols * 2, arma::fill::zeros);

    auto row = 0;
    for (auto t : timestamps)
    {
        auto points = pointData->getPointsAtTime(t);

        auto col = 0;
        for (auto p : points) {
            result(row, col * 2) = p.x;
            result(row, col * 2 + 1) = p.y;
            col++;
        }
        row++;
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

////////////////////////////////////////////////////////

// TensorData




////////////////////////////////////////////////////////

//AnalogTimeSeries


#endif //WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
