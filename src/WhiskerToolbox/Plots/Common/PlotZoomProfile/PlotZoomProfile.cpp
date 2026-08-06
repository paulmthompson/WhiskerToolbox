/**
 * @file PlotZoomProfile.cpp
 * @brief Implementation of optional wheel-zoom repaint profiling.
 */

#include "PlotZoomProfile.hpp"

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <string>
#include <string_view>

namespace Neuralyzer::Plots {

namespace {

[[nodiscard]] bool envFlagEnabled(char const * value) {
    if (value == nullptr || value[0] == '\0') {
        return false;
    }
    return std::string_view{value} == "1" || std::string_view{value} == "true";
}

}// namespace

bool PlotZoomProfile::enabled() {
    static bool const k_enabled = envFlagEnabled(std::getenv("NEURALYZER_PLOT_ZOOM_PROFILE"));
    return k_enabled;
}

PlotZoomProfile & PlotZoomProfile::instance() {
    static PlotZoomProfile profiler;
    return profiler;
}

void PlotZoomProfile::beginWheel() {
    if (!enabled()) {
        return;
    }
    ++_generation;
    _pending_generation = _generation;
    _wheel_start = std::chrono::steady_clock::now();
}

void PlotZoomProfile::endWheelSync() {
    if (!enabled() || !_wheel_start.has_value()) {
        return;
    }

    auto const wheel_sync_us = elapsedMicros(*_wheel_start);
    _wheel_start.reset();

    spdlog::info(
            "[PlotZoomProfile] gen={} wheel_sync_us={}",
            _pending_generation,
            wheel_sync_us);
}

void PlotZoomProfile::beginPaintGL() {
    if (!enabled()) {
        return;
    }
    _paint_start = std::chrono::steady_clock::now();
}

void PlotZoomProfile::endPaintGL(bool rebuilt_scene) {
    if (!enabled() || !_paint_start.has_value()) {
        return;
    }

    auto const paint_gl_us = elapsedMicros(*_paint_start);
    _paint_start.reset();

    spdlog::info(
            "[PlotZoomProfile] gen={} paint_gl_us={} rebuild={}",
            _pending_generation,
            paint_gl_us,
            rebuilt_scene ? 1 : 0);
}

void PlotZoomProfile::beginAxisPaint(std::string_view axis_label) {
    if (!enabled()) {
        return;
    }
    _axis_label = axis_label;
    _axis_start = std::chrono::steady_clock::now();
}

void PlotZoomProfile::endAxisPaint() {
    if (!enabled() || !_axis_start.has_value()) {
        return;
    }

    auto const axis_paint_us = elapsedMicros(*_axis_start);
    auto const axis_label = _axis_label;
    _axis_start.reset();
    _axis_label = {};

    spdlog::info(
            "[PlotZoomProfile] gen={} axis_paint_us={} axis={}",
            _pending_generation,
            axis_paint_us,
            axis_label);
}

std::int64_t PlotZoomProfile::elapsedMicros(
        std::chrono::steady_clock::time_point start) {
    auto const elapsed = std::chrono::steady_clock::now() - start;
    return std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
}

PlotZoomProfileWheelScope::PlotZoomProfileWheelScope()
    : _active{PlotZoomProfile::enabled()} {
    if (_active) {
        PlotZoomProfile::instance().beginWheel();
    }
}

PlotZoomProfileWheelScope::~PlotZoomProfileWheelScope() {
    if (_active) {
        PlotZoomProfile::instance().endWheelSync();
    }
}

PlotZoomProfilePaintGLScope::PlotZoomProfilePaintGLScope()
    : _active{PlotZoomProfile::enabled()} {
    if (_active) {
        PlotZoomProfile::instance().beginPaintGL();
    }
}

PlotZoomProfilePaintGLScope::~PlotZoomProfilePaintGLScope() {
    if (_active) {
        PlotZoomProfile::instance().endPaintGL(_rebuilt_scene);
    }
}

void PlotZoomProfilePaintGLScope::setRebuiltScene(bool rebuilt_scene) {
    _rebuilt_scene = rebuilt_scene;
}

PlotZoomProfileAxisPaintScope::PlotZoomProfileAxisPaintScope(std::string_view axis_label)
    : _active{PlotZoomProfile::enabled()} {
    if (_active) {
        PlotZoomProfile::instance().beginAxisPaint(axis_label);
    }
}

PlotZoomProfileAxisPaintScope::~PlotZoomProfileAxisPaintScope() {
    if (_active) {
        PlotZoomProfile::instance().endAxisPaint();
    }
}

}// namespace Neuralyzer::Plots
