#ifndef COREPLOTTING_MAPPERS_RASTERMAPPER_HPP
#define COREPLOTTING_MAPPERS_RASTERMAPPER_HPP

#include "MappedElement.hpp"
#include "MappedLineView.hpp"
#include "MapperConcepts.hpp"
#include "Layout/SeriesLayout.hpp"
#include "Layout/LayoutTransform.hpp"

#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <ranges>
#include <vector>

namespace CorePlotting {

/**
 * @brief Mapper for raster plots (PSTH/spike raster style visualization)
 * 
 * Transforms event data with relative time positioning. Unlike TimeSeriesMapper
 * which uses absolute time for X coordinates, RasterMapper computes X as the
 * relative offset from a reference event.
 * 
 * This mapper is used for:
 * - PSTH raster plots: events aligned to stimulus/behavior
 * - Multi-trial visualizations: each trial in a separate row
 * - Event-centered analysis: spikes aligned to action potentials
 * 
 * Key difference from TimeSeriesMapper:
 * - X = (event_time - reference_time), not absolute time
 * - Y = row_index (from RowLayoutStrategy), not stacked position
 */
namespace RasterMapper {

// ============================================================================
// Single Row Mapping: Events → MappedElement range with relative time
// ============================================================================

/**
 * @brief Map events to relative time positions (single row/trial)
 * 
 * Transforms events into (x, y, entity_id) where:
 * - X = event_time - reference_time (relative offset)
 * - Y = layout.y_transform.offset (row center)
 * 
 * @param series Event series to map
 * @param layout Layout allocation for Y positioning (from RowLayoutStrategy)
 * @param time_frame TimeFrame for index→time conversion
 * @param reference_time Reference time for relative calculation (e.g., stimulus onset)
 * @return Range of MappedElement
 */
[[nodiscard]] inline auto mapEventsRelative(
    DigitalEventSeries const & series,
    SeriesLayout const & layout,
    TimeFrame const & time_frame,
    TimeFrameIndex reference_time
) {
    float const y_center = layout.y_transform.offset;
    int const ref_abs_time = time_frame.getTimeAtIndex(reference_time);
    
    return series.view()
        | std::views::transform([&time_frame, y_center, ref_abs_time](auto const & event_with_id) {
            int abs_time = time_frame.getTimeAtIndex(event_with_id.event_time);
            float relative_time = static_cast<float>(abs_time - ref_abs_time);
            return MappedElement{relative_time, y_center, event_with_id.entity_id};
        });
}

/**
 * @brief Map events in a time window around reference
 * 
 * Only includes events within [reference - window_before, reference + window_after].
 * 
 * @param series Event series to map
 * @param layout Layout allocation for Y positioning
 * @param time_frame TimeFrame for conversion
 * @param reference_time Reference time (center of window)
 * @param window_before Time units before reference to include
 * @param window_after Time units after reference to include
 * @return Range of MappedElement for events in window
 */
[[nodiscard]] inline auto mapEventsInWindow(
    DigitalEventSeries const & series,
    SeriesLayout const & layout,
    TimeFrame const & time_frame,
    TimeFrameIndex reference_time,
    int window_before,
    int window_after
) {
    float const y_center = layout.y_transform.offset;
    int const ref_abs_time = time_frame.getTimeAtIndex(reference_time);
    int const window_start = ref_abs_time - window_before;
    int const window_end = ref_abs_time + window_after;
    
    return series.view()
        | std::views::filter([&time_frame, window_start, window_end](auto const & event_with_id) {
            int abs_time = time_frame.getTimeAtIndex(event_with_id.event_time);
            return abs_time >= window_start && abs_time <= window_end;
        })
        | std::views::transform([&time_frame, y_center, ref_abs_time](auto const & event_with_id) {
            int abs_time = time_frame.getTimeAtIndex(event_with_id.event_time);
            float relative_time = static_cast<float>(abs_time - ref_abs_time);
            return MappedElement{relative_time, y_center, event_with_id.entity_id};
        });
}

// ============================================================================
// Multi-Row (Trial) Mapping
// ============================================================================

/**
 * @brief Configuration for multi-trial raster mapping
 */
struct TrialConfig {
    DigitalEventSeries const * series;  ///< Event series for this trial
    TimeFrameIndex reference_time;       ///< Reference event for this trial
    SeriesLayout layout;                 ///< Layout (Y position) for this trial
};

/**
 * @brief Map multiple trials to a combined MappedElement vector
 * 
 * Each trial has its own reference time and Y position. All events
 * from all trials are combined into a single output suitable for
 * building a glyph batch.
 * 
 * @param trials Vector of trial configurations
 * @param time_frame TimeFrame for index→time conversion
 * @return Vector of MappedElement (all trials combined)
 */
[[nodiscard]] inline std::vector<MappedElement> mapTrials(
    std::vector<TrialConfig> const & trials,
    TimeFrame const & time_frame
) {
    std::vector<MappedElement> result;
    
    // Estimate total capacity
    size_t total_events = 0;
    for (auto const & trial : trials) {
        if (trial.series) {
            total_events += trial.series->size();
        }
    }
    result.reserve(total_events);
    
    // Map each trial
    for (auto const & trial : trials) {
        if (!trial.series) continue;
        
        for (auto const & elem : mapEventsRelative(*trial.series, trial.layout, time_frame, trial.reference_time)) {
            result.push_back(elem);
        }
    }
    
    return result;
}

/**
 * @brief Map trials with window filtering
 * 
 * Only includes events within the specified window around each
 * trial's reference time.
 * 
 * @param trials Vector of trial configurations
 * @param time_frame TimeFrame for conversion
 * @param window_before Time units before reference
 * @param window_after Time units after reference
 * @return Vector of MappedElement (filtered by window)
 */
[[nodiscard]] inline std::vector<MappedElement> mapTrialsInWindow(
    std::vector<TrialConfig> const & trials,
    TimeFrame const & time_frame,
    int window_before,
    int window_after
) {
    std::vector<MappedElement> result;
    
    for (auto const & trial : trials) {
        if (!trial.series) continue;
        
        for (auto const & elem : mapEventsInWindow(
                *trial.series, trial.layout, time_frame, 
                trial.reference_time, window_before, window_after)) {
            result.push_back(elem);
        }
    }
    
    return result;
}

// ============================================================================
// Layout Helpers for Raster Plots
// ============================================================================

/**
 * @brief Compute Y position for a specific row in a raster plot
 * 
 * Utility for manual row positioning without full LayoutEngine.
 * 
 * @param row_index Index of the row (0-based)
 * @param total_rows Total number of rows
 * @param y_min Minimum Y coordinate
 * @param y_max Maximum Y coordinate
 * @return Y center position for this row
 */
[[nodiscard]] inline float computeRowYCenter(
    int row_index,
    int total_rows,
    float y_min = -1.0f,
    float y_max = 1.0f
) {
    if (total_rows <= 0) return (y_min + y_max) / 2.0f;
    
    float const total_height = y_max - y_min;
    float const row_height = total_height / static_cast<float>(total_rows);
    
    // Row 0 at top (y_max), row N-1 at bottom (y_min)
    return y_max - (static_cast<float>(row_index) + 0.5f) * row_height;
}

/**
 * @brief Create SeriesLayout for a raster row
 * 
 * @param row_index Index of the row (0-based)
 * @param total_rows Total number of rows
 * @param series_id Series identifier
 * @param y_min Minimum Y coordinate
 * @param y_max Maximum Y coordinate
 * @return SeriesLayout configured for this row
 */
[[nodiscard]] inline SeriesLayout makeRowLayout(
    int row_index,
    int total_rows,
    std::string series_id,
    float y_min = -1.0f,
    float y_max = 1.0f
) {
    float const total_height = y_max - y_min;
    float const row_height = total_height / static_cast<float>(total_rows);
    float const y_center = computeRowYCenter(row_index, total_rows, y_min, y_max);
    float const half_height = row_height / 2.0f;
    
    return SeriesLayout{
        std::move(series_id),
        LayoutTransform{y_center, half_height},
        row_index
    };
}

// ============================================================================
// Materialized Versions
// ============================================================================

/**
 * @brief Map events relative to reference and return as vector
 * 
 * @param series Event series
 * @param layout Layout allocation
 * @param time_frame TimeFrame for conversion
 * @param reference_time Reference time
 * @return Vector of MappedElement
 */
[[nodiscard]] inline std::vector<MappedElement> mapEventsRelativeToVector(
    DigitalEventSeries const & series,
    SeriesLayout const & layout,
    TimeFrame const & time_frame,
    TimeFrameIndex reference_time
) {
    std::vector<MappedElement> result;
    result.reserve(series.size());
    
    for (auto const & elem : mapEventsRelative(series, layout, time_frame, reference_time)) {
        result.push_back(elem);
    }
    
    return result;
}

} // namespace RasterMapper

} // namespace CorePlotting

#endif // COREPLOTTING_MAPPERS_RASTERMAPPER_HPP
