/**
 * @file HeatmapPropertiesLayout.test.cpp
 * @brief Integration test reproducing the HeatmapPropertiesWidget scaling section layout
 *
 * Recreates the exact Section + ColormapControls + EstimationMethodControls
 * hierarchy that HeatmapPropertiesWidget._setupScalingSection() builds, then
 * tests whether the LayoutSanityChecker detects violations when the section
 * is expanded and the color range is switched to Manual at constrained widths.
 */

#include "Collapsible_Widget/Section.hpp"
#include "LayoutTesting/LayoutSanityChecker.hpp"
#include "Plots/Common/ColormapControls/ColormapControls.hpp"
#include "Plots/Common/RateEstimationControls/EstimationMethodControls.hpp"
#include "Plots/Common/RateEstimationControls/ScalingModeControls.hpp"

#include <QApplication>
#include <QTimer>
#include <QVBoxLayout>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {

QApplication * ensureQApp() {
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }
    return qobject_cast<QApplication *>(QApplication::instance());
}

/// Wait for Section animation to complete (default 100ms, give it 250ms)
void waitForAnimation(int ms = 250) {
    QTimer timer;
    timer.setSingleShot(true);
    timer.start(ms);
    while (timer.isActive()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
}

auto significantViolations(QWidget * root) -> std::vector<LayoutViolation> {
    auto all = LayoutSanityChecker::getViolations(root);
    std::erase_if(all, [](auto const & v) {
        return v.severity == LayoutViolation::Severity::Info;
    });
    return all;
}

/**
 * @brief Build the same widget hierarchy as HeatmapPropertiesWidget::_setupScalingSection()
 */
struct ScalingSectionFixture {
    QWidget * container;
    Section * section;
    EstimationMethodControls * estimation_controls;
    ScalingModeControls * scaling_controls;
    ColormapControls * colormap_controls;
};

auto buildScalingSection() -> ScalingSectionFixture {
    auto * container = new QWidget();
    container->setObjectName("HeatmapPropertiesWidget");
    auto * main_layout = new QVBoxLayout(container);
    main_layout->setContentsMargins(4, 4, 4, 4);

    auto * section = new Section(container, "Rate Estimation & Scaling");

    auto * estimation_controls = new EstimationMethodControls(section);
    auto * scaling_controls = new ScalingModeControls(section);
    auto * colormap_controls = new ColormapControls(section);

    auto * layout = new QVBoxLayout();
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(estimation_controls);
    layout->addWidget(scaling_controls);
    layout->addWidget(colormap_controls);

    section->setContentLayout(*layout);
    main_layout->addWidget(section);
    main_layout->addStretch();

    return {container, section, estimation_controls, scaling_controls, colormap_controls};
}

}// namespace


TEST_CASE("Scaling section Manual mode — violations detected at 250px (user scenario)",
          "[ui][layout][heatmap]") {
    ensureQApp();

    auto [container, section, estimation, scaling, colormap] = buildScalingSection();

    container->resize(250, 600);
    container->show();
    QCoreApplication::processEvents();

    // Expand the section
    section->toggle(true);
    waitForAnimation();

    // Switch to Manual — spinboxes appear, smushing expected at this width
    colormap->setColorRangeMode(ColorRangeConfig::Mode::Manual);
    QCoreApplication::processEvents();

    auto violations = significantViolations(container);

    for (auto const & v: violations) {
        WARN(v.widgetPath.toStdString() + " — " + v.description.toStdString());
    }

    delete container;

    // The checker must detect at least one violation at 250px with Manual mode
    CHECK_FALSE(violations.empty());
}


TEST_CASE("Scaling section Manual mode — no content overflow at 500px",
          "[ui][layout][heatmap]") {
    ensureQApp();

    auto [container, section, estimation, scaling, colormap] = buildScalingSection();

    container->resize(500, 800);
    container->show();
    QCoreApplication::processEvents();

    section->toggle(true);
    waitForAnimation();

    colormap->setColorRangeMode(ColorRangeConfig::Mode::Manual);
    QCoreApplication::processEvents();

    // The Section needs to recalculate its content height after spinboxes appear
    section->updateHeights();
    waitForAnimation();

    auto all = LayoutSanityChecker::getViolations(container);

    // At 500px wide, there should be no content overflow violations (checks 4-6).
    // Minor 1-2 pixel height mismatches from Section layout are tolerated.
    std::vector<LayoutViolation> content_overflow;
    for (auto const & v: all) {
        if (v.description.contains(QStringLiteral("truncated")) ||
            v.description.contains(QStringLiteral("too narrow"))) {
            content_overflow.push_back(v);
        }
    }

    for (auto const & v: content_overflow) {
        WARN(v.widgetPath.toStdString() + " — " + v.description.toStdString());
    }

    delete container;

    CHECK(content_overflow.empty());
}


TEST_CASE("Scaling section — collapsed section has no visible violations",
          "[ui][layout][heatmap]") {
    ensureQApp();

    auto [container, section, estimation, scaling, colormap] = buildScalingSection();

    // Section starts collapsed by default
    container->resize(250, 600);
    container->show();
    QCoreApplication::processEvents();

    // Wait for initial collapse animation to finish
    waitForAnimation();

    auto violations = significantViolations(container);

    for (auto const & v: violations) {
        WARN("COLLAPSED: " + v.widgetPath.toStdString() + " — " +
             v.description.toStdString() +
             " (actual: " + std::to_string(v.actual.width()) + "x" +
             std::to_string(v.actual.height()) + ")");
    }

    delete container;

    CHECK(violations.empty());
}


TEST_CASE("Scaling section Manual mode — width sweep",
          "[ui][layout][heatmap]") {
    ensureQApp();

    auto [container, section, estimation, scaling, colormap] = buildScalingSection();

    container->show();
    QCoreApplication::processEvents();

    section->toggle(true);
    waitForAnimation();

    std::vector<std::string> all_violations_log;

    for (int w = 150; w <= 500; w += 25) {
        container->resize(w, 600);
        QCoreApplication::processEvents();

        colormap->setColorRangeMode(ColorRangeConfig::Mode::Manual);
        QCoreApplication::processEvents();

        auto violations = significantViolations(container);
        for (auto const & v: violations) {
            std::string const entry = "w=" + std::to_string(w) + " " +
                                      v.widgetPath.toStdString() + " — " +
                                      v.description.toStdString();
            all_violations_log.push_back(entry);
        }

        colormap->setColorRangeMode(ColorRangeConfig::Mode::Auto);
        QCoreApplication::processEvents();
    }

    for (auto const & entry: all_violations_log) {
        WARN(entry);
    }

    delete container;

    WARN("Total significant violations across sweep: " + std::to_string(all_violations_log.size()));
}
