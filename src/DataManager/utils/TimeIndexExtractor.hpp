#ifndef TIME_INDEX_EXTRACTOR_HPP
#define TIME_INDEX_EXTRACTOR_HPP

/**
 * @file TimeIndexExtractor.hpp
 * @brief Utility for extracting time indices from any DataManager data type.
 *
 * This decouples UI code (e.g. TensorDesigner) from needing per-type
 * includes and dispatch logic. The visitor lives in DataManager — which
 * already depends on every data type — so callers only need this header.
 *
 * @see tensor_column_refactor_proposal.md §4.4 Option A
 */

#include "TimeFrame/TimeFrame.hpp"
#include "datamanager_export.h"

#include <memory>
#include <string>
#include <vector>

class DataManager;

/**
 * @brief Result of time-index extraction from a data source.
 *
 * Contains the ordered vector of TimeFrameIndex values and the shared
 * TimeFrame associated with that data source.
 */
struct DATAMANAGER_EXPORT TimeIndexResult {
    std::vector<TimeFrameIndex> indices;
    std::shared_ptr<TimeFrame> time_frame;

    [[nodiscard]] bool empty() const noexcept { return indices.empty(); }
    [[nodiscard]] std::size_t size() const noexcept { return indices.size(); }
};

/**
 * @brief Extract time indices from any data type stored under @p key.
 *
 * Dispatches via the DataTypeVariant to produce a uniform result:
 *
 * | Source type              | Indices returned                 |
 * |--------------------------|----------------------------------|
 * | AnalogTimeSeries         | getTimeSeries()                  |
 * | DigitalEventSeries       | event timestamps                 |
 * | DigitalIntervalSeries    | interval start times             |
 * | MaskData / LineData / PointData | getTimesWithData()        |
 * | MediaData / RaggedAnalog / TensorData | empty result        |
 *
 * @param dm  The DataManager to query (non-const because getData<T>() is non-const).
 * @param key The data key to look up.
 * @return TimeIndexResult with indices and time_frame. Empty if the key
 *         does not exist or the type has no meaningful time indices.
 */
DATAMANAGER_EXPORT TimeIndexResult extractTimeIndices(
        DataManager & dm,
        std::string const & key);

#endif // TIME_INDEX_EXTRACTOR_HPP
