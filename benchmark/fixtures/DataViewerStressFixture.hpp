/**
 * @file DataViewerStressFixture.hpp
 * @brief Synthetic setup and horizontal scroll probes for DataViewer OpenGLWidget heaptrack runs.
 */

#ifndef DATAVIEWER_STRESS_FIXTURE_HPP
#define DATAVIEWER_STRESS_FIXTURE_HPP

#include "Rendering/OpenGLWidget.hpp"
#include "adhoc/rendering/SyntheticDataFactory.hpp"

#include <QCoreApplication>
#include <QEventLoop>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

namespace Neuralyzer::Benchmark {

/**
 * @brief Configuration for DataViewer allocation probe runs.
 */
struct DataViewerProbeConfig {
    std::size_t num_channels{1};
    std::size_t num_samples{10'000};
    std::size_t window_size{1'000};
    std::size_t scroll_frames{1};
    std::size_t scroll_step{1};
};

/** @brief Alias for backward compatibility within the benchmark tree. */
using DataViewerStressConfig = DataViewerProbeConfig;

/**
 * @brief Number of scene rebuilds expected for a probe configuration.
 * @post Returns one initial paint plus one rebuild per scroll iteration.
 */
[[nodiscard]] inline std::size_t expectedSceneRebuilds(DataViewerProbeConfig const & config) {
    return 1 + config.scroll_frames;
}

/**
 * @brief Print probe parameters and theoretical expectations to stdout.
 */
inline void printProbeManifest(DataViewerProbeConfig const & config) {
    std::cout << "DataViewer allocation probe\n";
    std::cout << "  channels=" << config.num_channels << '\n';
    std::cout << "  samples=" << config.num_samples << '\n';
    std::cout << "  window=" << config.window_size << '\n';
    std::cout << "  scroll_frames=" << config.scroll_frames << '\n';
    std::cout << "  scroll_step=" << config.scroll_step << '\n';
    std::cout << "  expected_scene_rebuilds=" << expectedSceneRebuilds(config) << '\n';
    std::cout << "  expected_frame1_vertex_gen_per_ch=O(scroll_step) if cache works\n";
    std::cout << "  expected_extract_allocs_per_rebuild_per_ch=1 x O(window)\n";
}

/**
 * @brief Populate an OpenGLWidget with synthetic TensorColumn analog series.
 * @pre @p widget must outlive @p data.
 * @post Master time frame and one analog series per channel are registered.
 */
inline void configureDataViewerStressWidget(
        OpenGLWidget & widget,
        BenchmarkSynthetic::TensorColumnResult const & data,
        DataViewerProbeConfig const & config) {
    widget.setMasterTimeFrame(data.time_frame);

    for (std::size_t ch = 0; ch < data.channels.size(); ++ch) {
        std::string const key = "voltage_" + std::to_string(ch + 1);
        widget.addAnalogTimeSeries(key, data.channels[ch]);
    }

    if (auto state = widget.stateShared()) {
        int64_t const window = static_cast<int64_t>(config.window_size);
        if (window > 0) {
            int64_t const end = std::min(window - 1, static_cast<int64_t>(config.num_samples) - 1);
            state->setTimeWindow(0, end);
        }
    }
}

/**
 * @brief Run a deterministic horizontal scroll probe through @p state and @p widget.
 * @pre A QApplication must exist and @p widget must be shown with a valid GL context.
 * @post Probe completed (clamped when the window exceeds sample extent).
 */
inline void runDataViewerHorizontalScrollStress(
        OpenGLWidget & widget,
        DataViewerState & state,
        DataViewerProbeConfig const & config) {
    int64_t const window = static_cast<int64_t>(config.window_size);
    if (window < 1) {
        return;
    }

    int64_t const max_start = static_cast<int64_t>(config.num_samples) - window;
    if (max_start < 0) {
        return;
    }

    state.setTimeWindow(0, window - 1);
    widget.update();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    for (std::size_t frame = 0; frame < config.scroll_frames; ++frame) {
        int64_t const offset = static_cast<int64_t>((frame + 1) * config.scroll_step);
        if (offset > max_start) {
            break;
        }

        int64_t const start = offset;
        int64_t const end = start + window - 1;
        state.setTimeWindow(start, end);
        widget.update();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    }
}

}// namespace Neuralyzer::Benchmark

#endif// DATAVIEWER_STRESS_FIXTURE_HPP
