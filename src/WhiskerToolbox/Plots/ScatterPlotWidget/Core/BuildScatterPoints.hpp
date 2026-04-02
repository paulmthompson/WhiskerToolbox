#ifndef BUILD_SCATTER_POINTS_HPP
#define BUILD_SCATTER_POINTS_HPP

/**
 * @file BuildScatterPoints.hpp
 * @brief Factory function for building scatter point data from two axis sources
 *
 * Given two ScatterAxisSource descriptors and a DataManager, this function
 * computes the paired (x, y, TimeFrameIndex) triples by joining on the shared
 * row type (TimeFrameIndex or ordinal position).
 *
 * @see ScatterPointData for the result structure
 * @see ScatterAxisSource for the source descriptor
 * @see SourceCompatibility for validation (should be checked before calling)
 */

#include "ScatterAxisSource.hpp"
#include "ScatterPointData.hpp"

class DataManager;

/**
 * @brief Build scatter point data from two axis sources
 *
 * @pre sources must be compatible (check with checkSourceCompatibility() first) (enforcement: runtime_check)
 *
 * For AnalogTimeSeries x AnalogTimeSeries: iterates over the intersection of valid
 * TimeFrameIndex values, applying temporal offset by shifting the index.
 *
 * For AnalogTimeSeries x TensorData (TimeFrameIndex rows): joins on shared
 * TimeFrameIndex values.
 *
 * For TensorData (Ordinal) x TensorData (Ordinal): uses positional pairing (row i
 * from each tensor). TimeFrameIndex is set to the row index.
 *
 * @param dm       DataManager to read data from
 * @param x_source X-axis data source descriptor
 * @param y_source Y-axis data source descriptor
 * @return ScatterPointData with matched (x, y, time_index) triples
 */
[[nodiscard]] ScatterPointData buildScatterPoints(
    DataManager & dm,
    ScatterAxisSource const & x_source,
    ScatterAxisSource const & y_source);

#endif  // BUILD_SCATTER_POINTS_HPP
