#ifndef PLOT_ZOOM_PROFILE_HPP
#define PLOT_ZOOM_PROFILE_HPP

/**
 * @file PlotZoomProfile.hpp
 * @brief Optional wheel-zoom repaint profiling for OpenGL plot widgets
 *
 * Enable at runtime with environment variable `NEURALYZER_PLOT_ZOOM_PROFILE=1`.
 *
 * Logs three timing buckets per wheel interaction:
 * - `wheel_sync_us` — synchronous work from wheelEvent through signal/axis sync
 * - `paint_gl_us` + `rebuild` — paintGL duration and whether rebuildScene ran
 * - `axis_paint_us` — QPainter axis repaint cost (may arrive after wheel_sync)
 *
 * Wheel and paint events are correlated via a monotonic `gen` counter.
 */

#include <chrono>
#include <cstdint>
#include <optional>
#include <string_view>

namespace Neuralyzer::Plots {

/**
 * @brief Runtime-gated zoom/paint profiler (singleton).
 */
class PlotZoomProfile {
public:
    /**
     * @brief True when `NEURALYZER_PLOT_ZOOM_PROFILE` is `1` or `true`.
     */
    [[nodiscard]] static bool enabled();

    /**
     * @brief Profiler singleton.
     */
    [[nodiscard]] static PlotZoomProfile & instance();

    /**
     * @brief Begin profiling a wheel interaction (increments generation).
     */
    void beginWheel();

    /**
     * @brief End synchronous wheel path and log `wheel_sync_us`.
     */
    void endWheelSync();

    /**
     * @brief Begin paintGL timing for the pending wheel generation.
     */
    void beginPaintGL();

    /**
     * @brief End paintGL timing and log `paint_gl_us` and `rebuild`.
     * @param rebuilt_scene True when rebuildScene() ran during this frame.
     */
    void endPaintGL(bool rebuilt_scene);

    /**
     * @brief Begin axis paintEvent timing.
     * @param axis_label Short label such as "horizontal" or "vertical".
     */
    void beginAxisPaint(std::string_view axis_label);

    /**
     * @brief End axis paintEvent timing and log `axis_paint_us`.
     */
    void endAxisPaint();

    /**
     * @brief Generation counter for the most recent wheel interaction.
     */
    [[nodiscard]] std::uint64_t pendingGeneration() const { return _pending_generation; }

private:
    PlotZoomProfile() = default;

    [[nodiscard]] static std::int64_t elapsedMicros(
            std::chrono::steady_clock::time_point start);

    std::uint64_t _generation{0};
    std::uint64_t _pending_generation{0};
    std::optional<std::chrono::steady_clock::time_point> _wheel_start;
    std::optional<std::chrono::steady_clock::time_point> _paint_start;
    std::optional<std::chrono::steady_clock::time_point> _axis_start;
    std::string_view _axis_label;
};

/**
 * @brief RAII scope for wheelEvent synchronous profiling.
 */
class PlotZoomProfileWheelScope {
public:
    PlotZoomProfileWheelScope();
    ~PlotZoomProfileWheelScope();

    PlotZoomProfileWheelScope(PlotZoomProfileWheelScope const &) = delete;
    PlotZoomProfileWheelScope & operator=(PlotZoomProfileWheelScope const &) = delete;

private:
    bool const _active;
};

/**
 * @brief RAII scope for paintGL profiling.
 */
class PlotZoomProfilePaintGLScope {
public:
    PlotZoomProfilePaintGLScope();
    ~PlotZoomProfilePaintGLScope();

    PlotZoomProfilePaintGLScope(PlotZoomProfilePaintGLScope const &) = delete;
    PlotZoomProfilePaintGLScope & operator=(PlotZoomProfilePaintGLScope const &) = delete;

    /**
     * @brief Record whether rebuildScene() executed in this paint.
     */
    void setRebuiltScene(bool rebuilt_scene);

private:
    bool const _active;
    bool _rebuilt_scene{false};
};

/**
 * @brief RAII scope for axis paintEvent profiling.
 */
class PlotZoomProfileAxisPaintScope {
public:
    /**
     * @param axis_label Short label such as "horizontal" or "vertical".
     */
    explicit PlotZoomProfileAxisPaintScope(std::string_view axis_label);
    ~PlotZoomProfileAxisPaintScope();

    PlotZoomProfileAxisPaintScope(PlotZoomProfileAxisPaintScope const &) = delete;
    PlotZoomProfileAxisPaintScope & operator=(PlotZoomProfileAxisPaintScope const &) = delete;

private:
    bool const _active;
};

}// namespace Neuralyzer::Plots

#endif// PLOT_ZOOM_PROFILE_HPP
