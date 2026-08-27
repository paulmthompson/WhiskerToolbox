/**
 * @file DataViewerViewInteraction.benchmark.cpp
 * @brief DataViewer allocation probes for local heaptrack regression checks.
 *
 * Run directly or via record_benchmark_baselines / check_benchmark_regressions.
 * Each registered probe invokes this executable with fixed CLI arguments.
 */

#include "fixtures/DataViewerStressFixture.hpp"
#include "fixtures/QtOpenGLBenchmarkFixture.hpp"

#include <QApplication>
#include <QCoreApplication>

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

/**
 * @brief Print CLI usage and named probe recipes.
 */
void printUsage(char const * program) {
    std::cout << "Usage: " << program
              << " [channels samples window scroll_frames scroll_step]\n\n";
    std::cout << "Defaults: 1 10000 1000 1 1\n\n";
    std::cout << "Named probe recipes (for manual heaptrack runs):\n";
    std::cout << "  1ch-init:      " << program << " 1 10000 1000 0 1\n";
    std::cout << "  1ch-scroll1:   " << program << " 1 10000 1000 1 1\n";
    std::cout << "  2ch-scroll1:   " << program << " 2 10000 1000 1 1\n";
}

/**
 * @brief Run horizontal scroll probe on OpenGLWidget with synthetic multi-channel data.
 * @pre A QApplication must exist.
 * @post Probe completed.
 */
void runDataViewerAllocationProbe(Neuralyzer::Benchmark::DataViewerProbeConfig const & config) {
    auto const synthetic = BenchmarkSynthetic::createTensorColumnSeries(
            config.num_channels, config.num_samples);

    auto widget = std::make_unique<OpenGLWidget>();
    Neuralyzer::Benchmark::configureDataViewerStressWidget(*widget, synthetic, config);

    widget->resize(1920, 1080);

    if (!Neuralyzer::Benchmark::showOffscreenOpenGLWidget(*widget)) {
        Neuralyzer::Benchmark::printOpenGLWidgetContextStatus(*widget);
        throw std::runtime_error(
                "OpenGLWidget failed to acquire a valid OpenGL context — "
                "check QT_QPA_PLATFORM and display setup");
    }

    Neuralyzer::Benchmark::printOpenGLWidgetContextStatus(*widget);

    auto state = widget->stateShared();
    if (!state) {
        throw std::runtime_error("OpenGLWidget has no DataViewerState");
    }

    Neuralyzer::Benchmark::runDataViewerHorizontalScrollStress(*widget, *state, config);
}

}// namespace

/**
 * @brief Entry point for DataViewer allocation probes.
 */
int main(int argc, char ** argv) {
    try {
        if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            printUsage(argv[0]);
            return 0;
        }

        Neuralyzer::Benchmark::initializeQtPlatformForOpenGLBenchmarks();

        QApplication const app(argc, argv);

        Neuralyzer::Benchmark::DataViewerProbeConfig config;
        if (argc > 1) {
            config.num_channels = static_cast<std::size_t>(std::strtoul(argv[1], nullptr, 10));
        }
        if (argc > 2) {
            config.num_samples = static_cast<std::size_t>(std::strtoul(argv[2], nullptr, 10));
        }
        if (argc > 3) {
            config.window_size = static_cast<std::size_t>(std::strtoul(argv[3], nullptr, 10));
        }
        if (argc > 4) {
            config.scroll_frames = static_cast<std::size_t>(std::strtoul(argv[4], nullptr, 10));
        }
        if (argc > 5) {
            config.scroll_step = static_cast<std::size_t>(std::strtoul(argv[5], nullptr, 10));
        }

        Neuralyzer::Benchmark::printProbeManifest(config);
        runDataViewerAllocationProbe(config);
        std::cout << "Probe complete.\n";
    } catch (std::exception const & ex) {
        std::cerr << "Probe failed: " << ex.what() << '\n';
        return 1;
    } catch (...) {
        return 1;
    }

    return 0;
}
