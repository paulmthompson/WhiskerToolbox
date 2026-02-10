#ifndef COREPLOTTING_COORDINATETRANSFORM_AXISMAPPING_HPP
#define COREPLOTTING_COORDINATETRANSFORM_AXISMAPPING_HPP

/**
 * @file AxisMapping.hpp
 * @brief Describes the relationship between world (rendering) coordinates and
 *        domain-meaningful values, plus label formatting.
 *
 * The coordinate pipeline for a given axis is:
 *
 *     Screen pixels  ←→  World coords  ←→  Domain values  ←→  Label text
 *         (ViewState)        (AxisMapping)        (AxisMapping)
 *
 * ViewState handles screen↔world (zoom, pan, projection).
 * AxisMapping handles world↔domain and domain→label. Together they give the
 * full screen↔domain↔label chain.
 *
 * AxisMapping is a lightweight value type carrying three std::function members.
 * Factory functions produce common mapping patterns (identity, linear, trial
 * index, relative time, etc.) so that per-plot coordinate semantics are
 * explicit and reusable rather than buried in anonymous lambdas.
 *
 * @see CorePlotting/CoordinateTransform/ViewState.hpp
 */

#include <functional>
#include <string>

namespace CorePlotting {

/**
 * @brief Describes how one axis maps between world (rendering) coordinates
 *        and domain (semantically meaningful) values, plus how to format labels.
 *
 * This is a value type — copy/move freely. All three function members must be
 * non-null for a valid mapping; factory functions guarantee this.
 */
struct AxisMapping {
    /// Convert world coordinate → domain value (e.g., world_y 0.5 → trial 37)
    std::function<double(double world)> worldToDomain;

    /// Convert domain value → world coordinate (inverse of worldToDomain)
    std::function<double(double domain)> domainToWorld;

    /// Format a domain value as a display string (e.g., 37.0 → "Trial 37")
    std::function<std::string(double domain)> formatLabel;

    /// Optional axis title (e.g., "Trial", "Time (ms)")
    std::string title;

    // ── Convenience helpers ──

    /// Shorthand: world → domain → label
    [[nodiscard]] std::string label(double world) const {
        return formatLabel(worldToDomain(world));
    }

    /// Check whether all required functions are set
    [[nodiscard]] bool isValid() const {
        return worldToDomain && domainToWorld && formatLabel;
    }
};

// =============================================================================
// Factory Functions
// =============================================================================

/**
 * @brief Identity mapping: world == domain. Labels formatted as decimals.
 * @param title Optional axis title
 * @param decimals Number of decimal places in labels (default 1)
 */
AxisMapping identityAxis(std::string title = {}, int decimals = 1);

/**
 * @brief Linear mapping: domain = world * scale + offset
 *
 * Inverse: world = (domain - offset) / scale
 *
 * @param scale Multiplicative scale factor
 * @param offset Additive offset
 * @param title Optional axis title
 * @param decimals Number of decimal places in labels (default 1)
 */
AxisMapping linearAxis(double scale, double offset,
                       std::string title = {}, int decimals = 1);

/**
 * @brief Trial index axis for raster / event plots.
 *
 * Maps world Y ∈ [-1, 1] → trial ∈ [0, trial_count).
 *   domain = (world + 1) / 2 * trial_count
 *   world  = domain / trial_count * 2 - 1
 * Labels are integer trial indices: "0", "1", "2", …
 *
 * @param trial_count Number of trials (must be > 0)
 */
AxisMapping trialIndexAxis(std::size_t trial_count);

/**
 * @brief Relative time axis (world == domain, in milliseconds).
 *
 * Labels use sign-prefixed integer format: "-200", "0", "+500".
 */
AxisMapping relativeTimeAxis();

/**
 * @brief Analog signal axis with gain/offset and a unit string.
 *
 * domain = world * gain + offset
 * Labels: "1.23 mV", "-0.50 µV", etc.
 *
 * @param gain Multiplicative gain
 * @param offset Additive offset
 * @param unit Unit suffix appended to labels
 * @param decimals Number of decimal places in labels (default 2)
 */
AxisMapping analogAxis(double gain, double offset,
                       std::string unit, int decimals = 2);

}  // namespace CorePlotting

#endif  // COREPLOTTING_COORDINATETRANSFORM_AXISMAPPING_HPP
