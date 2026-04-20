#ifndef TIMESCROLLBAR_PLAYBACK_HPP
#define TIMESCROLLBAR_PLAYBACK_HPP

/**
 * @file TimeScrollBarPlayback.hpp
 * @brief Discrete FPS presets and timer math for TimeScrollBar playback
 *
 * Used by TimeScrollBar (QTimer period) and TimeScrollBarState (snapping
 * persisted values to the same preset ladder as the UI).
 */

#include <array>
#include <cmath>
#include <cstddef>

namespace time_scroll_bar {

/// Discrete playback rates shown in the timeline UI (Phase 3 triage plan).
inline constexpr std::array<float, 9> kFpsPresets{
        0.5F, 1.F, 2.F, 5.F, 10.F, 25.F, 50.F, 100.F, 200.F};

/**
 * @brief Index of the preset closest to @p fps.
 * @pre @p fps is finite (non-finite values map to index 0 as a safe default)
 */
[[nodiscard]] inline std::size_t presetIndexForTargetFps(float fps) noexcept {
    if (!std::isfinite(fps)) {
        return 0;
    }
    std::size_t best = 0;
    float best_diff = std::abs(fps - kFpsPresets[0]);
    for (std::size_t i = 1; i < kFpsPresets.size(); ++i) {
        float const d = std::abs(fps - kFpsPresets[i]);
        if (d < best_diff) {
            best_diff = d;
            best = i;
        }
    }
    return best;
}

/**
 * @brief Snap an arbitrary rate to the nearest discrete preset.
 */
[[nodiscard]] inline float snapTargetFpsToPreset(float fps) noexcept {
    return kFpsPresets[presetIndexForTargetFps(fps)];
}

/**
 * @brief Milliseconds between timer ticks for one-frame-per-tick playback.
 * @post Return value is in `[1, 2'000'000]` so QTimer stays within practical bounds.
 */
[[nodiscard]] inline int timerPeriodMsForTargetFps(float target_fps) noexcept {
    if (!std::isfinite(target_fps) || target_fps <= 0.F) {
        return 2000;
    }
    auto const ms = std::lround(1000.0 / static_cast<double>(target_fps));
    if (ms < 1) {
        return 1;
    }
    if (ms > 2'000'000) {
        return 2'000'000;
    }
    return static_cast<int>(ms);
}

}// namespace time_scroll_bar

#endif// TIMESCROLLBAR_PLAYBACK_HPP
