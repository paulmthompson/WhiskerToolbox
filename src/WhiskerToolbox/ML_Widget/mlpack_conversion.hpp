
#ifndef WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
#define WHISKERTOOLBOX_MLPACK_CONVERSION_HPP

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
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
        std::vector<std::size_t>& timestamps,
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
        std::vector<std::size_t>& timestamps)
{

    const size_t numRows = timestamps.size();
    const size_t numCols = pointData->getMaxPoints();

    arma::Mat<double> result(numRows, numCols * 2, arma::fill::zeros);

    auto row = 0;
    for (auto t : timestamps)
    {
        auto points = pointData->getPointsAtTime(t);

        if (points.empty()) {
            result(row, 0) = arma::datum::nan;
            row++;
            continue;
        }

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
        std::vector<std::size_t>& timestamps,
        std::shared_ptr<PointData> pointData)
{

    std::vector<std::vector<Point2D<float>>> points;

    for (std::size_t row = 0; row < matrix.n_rows; ++row) {
        points.push_back(std::vector<Point2D<float>>());
        for (std::size_t col = 0; col < matrix.n_cols; col += 2) {
            if (matrix(row, col) != 0.0 || matrix(row, col + 1) != 0.0) {
                points.back().emplace_back(Point2D<float>{
                        static_cast<float>(matrix(row, col)),
                        static_cast<float>(matrix(row, col + 1))
                });
            }
        }
    }
    pointData->overwritePointsAtTimes(timestamps, points);
}

////////////////////////////////////////////////////////

// TensorData




////////////////////////////////////////////////////////

//AnalogTimeSeries

/**
 * Convert an AnalogTimeSeries to an mlpack row vector
 * @param analogTimeSeries The AnalogTimeSeries to convert
 * @param timestamps The timestamps to convert
 * @return arma::Row<double> The mlpack row vector
 */
inline arma::Row<double> convertAnalogTimeSeriesToMlpackArray(
        const std::shared_ptr<AnalogTimeSeries>& analogTimeSeries,
        std::vector<std::size_t>& timestamps)
{
    auto length = timestamps.size();
    arma::Row<double> result(length, arma::fill::zeros);

    const auto& data = analogTimeSeries->getAnalogTimeSeries();
    const auto& time = analogTimeSeries->getTimeSeries();

    for (std::size_t i = 0; i < length; ++i) {
        auto it = std::find(time.begin(), time.end(), timestamps[i]);
        if (it != time.end()) {
            result[i] = data[std::distance(time.begin(), it)];
        } else {
            result[i] = arma::datum::nan;
        }
    }

    return result;
}

/**
 * Update an AnalogTimeSeries from an mlpack row vector
 * @param array The mlpack row vector
 * @param timestamps The timestamps to update
 * @param analogTimeSeries The AnalogTimeSeries to update
 */
inline void updateAnalogTimeSeriesFromMlpackArray(
        const arma::Row<double>& array,
        std::vector<std::size_t>& timestamps,
        std::shared_ptr<AnalogTimeSeries> analogTimeSeries)
{
    std::vector<float> data(array.n_elem);

    analogTimeSeries->overwriteAtTimes(data, timestamps);
}

#endif //WHISKERTOOLBOX_MLPACK_CONVERSION_HPP
