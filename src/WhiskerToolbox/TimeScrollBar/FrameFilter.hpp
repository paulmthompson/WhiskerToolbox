/**
 * @file FrameFilter.hpp
 * @brief Frame-skip filter interface and concrete implementations for TimeScrollBar playback.
 *
 * Provides an abstract `FrameFilter` that the TimeScrollBar queries on every frame
 * advance to decide whether to skip a frame. The primary concrete implementation,
 * `DataKeyFrameFilter`, is backed by a `DigitalIntervalSeries` key in the
 * `DataManager` and skips frames that are (or are not) covered by an interval.
 *
 * @see TimeScrollBar for integration points (`_vidLoop`, `changeScrollBarValue`)
 * @see optimized_contact_triage_plan.md Phase 4 for design rationale
 */

#ifndef FRAME_FILTER_HPP
#define FRAME_FILTER_HPP

#include "TimeFrame/TimeFrameIndex.hpp"// TimeFrameIndex

#include <memory>
#include <string>

class DataManager;
class DigitalIntervalSeries;
class TimeFrame;

// =============================================================================
// Abstract Interface
// =============================================================================

/**
 * @brief Abstract frame-skip predicate for TimeScrollBar.
 *
 * Implement `shouldSkip()` to tell the scrollbar whether a given frame index
 * should be skipped during playback and arrow-key navigation.
 *
 * Implementations must be fast (called once per frame tick) and must NOT
 * mutate any shared state.
 */
class FrameFilter {
public:
    virtual ~FrameFilter() = default;

    /**
     * @brief Return true if this frame should be skipped.
     *
     * @param index Frame index expressed in the scrollbar's current TimeFrame.
     * @return true  → skip this frame (advance to the next one).
     * @return false → stop on this frame.
     */
    [[nodiscard]] virtual bool shouldSkip(TimeFrameIndex index) const = 0;
};

// =============================================================================
// DataKeyFrameFilter
// =============================================================================

/**
 * @brief Frame filter backed by a `DigitalIntervalSeries` key in `DataManager`.
 *
 * When `skip_tracked` is `true` (the default), frames whose index falls inside
 * any interval of the series are skipped. Setting `skip_tracked` to `false`
 * inverts the behaviour: frames *outside* all intervals are skipped, which can
 * be used to confine playback to annotated regions.
 *
 * The filter is dynamic: it re-fetches the series on every call to
 * `shouldSkip()`, so changes made by hotkeys (Phase 2) are reflected
 * immediately during playback without needing to rebuild the filter.
 *
 * ## Usage
 * ```cpp
 * auto filter = std::make_unique<DataKeyFrameFilter>(data_manager, "tracked_regions");
 * filter->setTimeFrame(current_time_frame);
 * time_scroll_bar->setFrameFilter(std::move(filter));
 * ```
 */
class DataKeyFrameFilter : public FrameFilter {
public:
    /**
     * @brief Construct a DataKeyFrameFilter.
     *
     * @param data_manager  Shared DataManager owning the interval series.
     * @param key           DataManager key for the DigitalIntervalSeries.
     * @param skip_tracked  If true, skip frames inside any interval.
     *                      If false, skip frames outside all intervals.
     *
     * @pre data_manager is non-null.
     */
    DataKeyFrameFilter(std::shared_ptr<DataManager> data_manager,
                       std::string key,
                       bool skip_tracked = true);

    ~DataKeyFrameFilter() override = default;

    /**
     * @brief Set the TimeFrame used to interpret frame indices.
     *
     * Must be called (or updated) whenever the scrollbar switches TimeFrames so
     * that coordinate conversion inside `hasIntervalAtTime()` stays correct.
     *
     * @param time_frame The scrollbar's current TimeFrame (may be nullptr, in
     *                   which case raw index values are used for comparison).
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> time_frame);

    /**
     * @brief Return true if this frame should be skipped.
     *
     * Looks up the `DigitalIntervalSeries` from `DataManager` on each call.
     * Returns false (do not skip) if:
     * - The series cannot be found in DataManager, or
     * - No TimeFrame has been set.
     *
     * @param index Frame index in the scrollbar's TimeFrame coordinates.
     */
    [[nodiscard]] bool shouldSkip(TimeFrameIndex index) const override;

    /// @brief The DataManager key this filter observes.
    [[nodiscard]] std::string const & key() const noexcept { return _key; }

    /// @brief Whether the filter skips tracked (true) or untracked (false) frames.
    [[nodiscard]] bool skipsTracked() const noexcept { return _skip_tracked; }

private:
    std::shared_ptr<DataManager> _data_manager;
    std::string _key;
    std::shared_ptr<TimeFrame> _time_frame;
    bool _skip_tracked;
};

// =============================================================================
// Scan Utility
// =============================================================================

namespace frame_filter {

/**
 * @brief Scan from `start` in `direction` (+1 or -1) to find the first
 *        non-skipped frame within `[min_val, max_val]`.
 *
 * @param filter    The active FrameFilter.
 * @param start     Initial frame to test (inclusive).
 * @param direction +1 for forward scan, -1 for backward scan.
 * @param min_val   Lower bound of valid frame range (inclusive).
 * @param max_val   Upper bound of valid frame range (inclusive).
 *
 * @return The first non-skipped frame index, or -1 if all frames in the
 *         range are skipped (fully-tracked video guard).
 */
[[nodiscard]] int scanToNextNonSkipped(FrameFilter const & filter,
                                       int start,
                                       int direction,
                                       int min_val,
                                       int max_val) noexcept;

}// namespace frame_filter

#endif// FRAME_FILTER_HPP
