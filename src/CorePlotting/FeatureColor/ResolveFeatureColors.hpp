/**
 * @file ResolveFeatureColors.hpp
 * @brief Core resolution logic mapping plot points to per-point float color values
 *
 * Given a plot's point data (represented as TimeFrameIndex per point) and a
 * FeatureColorSourceDescriptor, resolves a float value for each point by joining
 * with AnalogTimeSeries, TensorData, or DigitalIntervalSeries.
 *
 * The returned vector of optional<float> is parallel to the point array:
 * - has_value() = color source had data for this point
 * - nullopt     = no data found (point keeps its default color)
 */

#ifndef COREPLOTTING_FEATURECOLOR_RESOLVEFEATURECOLORS_HPP
#define COREPLOTTING_FEATURECOLOR_RESOLVEFEATURECOLORS_HPP

#include "FeatureColorSourceDescriptor.hpp"

#include "TimeFrame/TimeFrameIndex.hpp"

#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <vector>

class DataManager;
class TimeFrame;

namespace CorePlotting::FeatureColor {

/**
 * @brief Resolve per-point float values from an external color data source
 *
 * The returned vector is parallel to @p point_times:
 * - result[i] has a value when the color source had data for point i
 * - result[i] is nullopt when no matching value was found
 *
 * Join strategies (selected automatically based on data type):
 * - AnalogTimeSeries:  ATS::getAtTime(point_time) per point
 * - TensorData (TFI):  Hash join — build TFI→row map, lookup per point
 * - TensorData (Ordinal): Positional join — row i maps to point i
 * - DigitalIntervalSeries: Containment check — 1.0 if inside interval, 0.0 otherwise
 *
 * @param dm              DataManager to look up the color data source
 * @param descriptor      Which data key / column to use
 * @param point_times     Per-point TimeFrameIndex values (parallel to scene glyph positions)
 * @param point_time_frame TimeFrame that @p point_times are expressed in
 *                         (needed for cross-timebase conversion with DIS)
 * @return Vector of optional<float> parallel to point_times
 *
 * @pre descriptor.data_key must be non-empty
 * @pre The key must resolve to AnalogTimeSeries, TensorData, or DigitalIntervalSeries
 */
[[nodiscard]] std::vector<std::optional<float>> resolveFeatureColors(
        DataManager & dm,
        FeatureColorSourceDescriptor const & descriptor,
        std::span<TimeFrameIndex const> point_times,
        std::shared_ptr<TimeFrame> const & point_time_frame);

}// namespace CorePlotting::FeatureColor

#endif// COREPLOTTING_FEATURECOLOR_RESOLVEFEATURECOLORS_HPP
