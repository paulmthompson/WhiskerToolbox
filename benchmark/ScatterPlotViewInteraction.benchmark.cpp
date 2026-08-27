/**
 * @file ScatterPlotViewInteraction.benchmark.cpp
 * @brief Fixed-loop ScatterPlot pan/zoom stress for local heaptrack regression checks.
 *
 * Run directly or via record_benchmark_baselines / check_benchmark_regressions.
 * Not a Google Benchmark executable; performs a deterministic pan/zoom stress loop.
 */

#include "Core/ScatterPlotState.hpp"
#include "Rendering/ScatterPlotOpenGLWidget.hpp"
#include "fixtures/ScatterPlotStressFixture.hpp"

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QEventLoop>
#include <QSurfaceFormat>

#include <cstddef>
#include <cstdlib>
#include <memory>

namespace {

constexpr std::size_t kDefaultPointCount = 10'000;
constexpr std::size_t kDefaultIterations = 5'000;

struct ScatterStressConfig {
    std::size_t point_count{kDefaultPointCount};
    std::size_t iteration_count{kDefaultIterations};
};

/**
 * @brief Initialize Qt platform and OpenGL defaults for ScatterPlotOpenGLWidget.
 */
void initializeQtForScatterStress() {
#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIsEmpty("WHISKER_USE_WAYLAND")) {
        qputenv("QT_QPA_PLATFORM", QByteArray("xcb"));
    }
#endif

    QSurfaceFormat format;
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setVersion(4, 3);
    format.setSamples(4);
    format.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(format);
}

/**
 * @brief Run pan/zoom stress on ScatterPlotOpenGLWidget with synthetic data.
 * @pre A QApplication must exist.
 * @post Stress loop completed.
 */
void runScatterPlotPanZoomStress(ScatterStressConfig const & config) {
    auto dm = Neuralyzer::Benchmark::makeScatterStressDataManager(config.point_count);
    auto state = std::make_shared<ScatterPlotState>();
    Neuralyzer::Benchmark::configureScatterStressSources(*state);

    auto widget = std::make_unique<ScatterPlotOpenGLWidget>();
    widget->setDataManager(std::move(dm));
    widget->setState(state);
    widget->resize(800, 600);
    widget->show();

    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    widget->update();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);

    for (std::size_t i = 0; i < config.iteration_count; ++i) {
        Neuralyzer::Benchmark::applyScatterStressPanZoom(*state, i);
        widget->update();
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    }
}

}// namespace

/**
 * @brief Entry point for ScatterPlot view interaction stress.
 */
int main(int argc, char ** argv) {
    initializeQtForScatterStress();

    QApplication const app(argc, argv);

    ScatterStressConfig config;

    if (argc > 1) {
        config.point_count = static_cast<std::size_t>(std::strtoul(argv[1], nullptr, 10));
    }
    if (argc > 2) {
        config.iteration_count = static_cast<std::size_t>(std::strtoul(argv[2], nullptr, 10));
    }

    runScatterPlotPanZoomStress(config);

    return 0;
}
