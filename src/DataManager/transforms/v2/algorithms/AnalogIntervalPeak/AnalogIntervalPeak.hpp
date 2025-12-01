#ifndef WHISKERTOOLBOX_V2_ANALOG_INTERVAL_PEAK_HPP
#define WHISKERTOOLBOX_V2_ANALOG_INTERVAL_PEAK_HPP

#include "transforms/v2/core/ElementTransform.hpp"

#include <memory>
#include <optional>
#include <rfl.hpp>
#include <rfl/json.hpp>

class AnalogTimeSeries;
class DigitalEventSeries;
class DigitalIntervalSeries;

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Parameters for analog interval peak detection
 * 
 * This transform finds peak (min/max) values in an analog signal within
 * time intervals defined by a digital interval series.
 * 
 * Example JSON:
 * ```json
 * {
 *   "peak_type": "maximum",
 *   "search_mode": "within_intervals"
 * }
 * ```
 */
struct AnalogIntervalPeakParams {
    /**
     * @brief Type of peak to find
     * - "minimum": Find minimum value within each interval
     * - "maximum": Find maximum value within each interval
     */
    std::optional<std::string> peak_type;

    /**
     * @brief Search mode for intervals
     * - "within_intervals": Search from interval start to interval end
     * - "between_starts": Search from one interval start to the next interval start
     */
    std::optional<std::string> search_mode;

    // Helper methods with defaults
    std::string getPeakType() const {
        return peak_type.value_or("maximum");
    }

    std::string getSearchMode() const {
        return search_mode.value_or("within_intervals");
    }

    bool isMaximum() const {
        return getPeakType() == "maximum";
    }

    bool isWithinIntervals() const {
        return getSearchMode() == "within_intervals";
    }
};

// ============================================================================
// Transform Implementation (Binary Container Transform)
// ============================================================================

/**
 * @brief Find peak values in analog signal within intervals
 * 
 * This is a **binary container transform** because:
 * - Requires temporal alignment between intervals and analog data
 * - Must search within time bounds of each interval
 * - Cannot be decomposed into simple element operations
 * 
 * The transform handles:
 * - TimeFrame conversion between interval and analog coordinate systems
 * - Searching for min/max within each interval
 * - Two search modes: within intervals or between interval starts
 * 
 * @param intervals Digital interval series defining search ranges
 * @param analog Analog time series to search for peaks
 * @param params Parameters controlling peak type and search mode
 * @param ctx Compute context for progress reporting and cancellation
 * @return Digital event series with peak timestamps (in interval coordinate system)
 */
std::shared_ptr<DigitalEventSeries> analogIntervalPeak(
        DigitalIntervalSeries const & intervals,
        AnalogTimeSeries const & analog,
        AnalogIntervalPeakParams const & params,
        ComputeContext const & ctx = {});

}// namespace WhiskerToolbox::Transforms::V2

#endif// WHISKERTOOLBOX_V2_ANALOG_INTERVAL_PEAK_HPP
