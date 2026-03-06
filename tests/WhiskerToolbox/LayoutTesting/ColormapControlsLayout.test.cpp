/**
 * @file ColormapControlsLayout.test.cpp
 * @brief Resize sweep and mode-toggle layout tests for ColormapControls
 *
 * Uses LayoutSanityChecker to verify that ColormapControls does not produce
 * layout violations at various sizes and mode combinations.
 */

#include "LayoutTesting/LayoutSanityChecker.hpp"
#include "Plots/Common/ColormapControls/ColormapControls.hpp"

#include <QApplication>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {

/**
 * @brief Ensure a QApplication exists for GUI tests (intentionally leaked)
 */
QApplication * ensureQApp() {
    if (!QApplication::instance()) {
        static int argc = 1;
        static char app_name[] = "test";
        static std::array<char *, 1> argv = {app_name};
        new QApplication(argc, argv.data());
    }
    return qobject_cast<QApplication *>(QApplication::instance());
}

/**
 * @brief Collect violations, excluding Info-level noise
 */
auto significantViolations(QWidget * root) -> std::vector<LayoutViolation> {
    auto all = LayoutSanityChecker::getViolations(root);
    std::erase_if(all, [](auto const & v) {
        return v.severity == LayoutViolation::Severity::Info;
    });
    return all;
}
}// namespace

TEST_CASE("ColormapControls layout resilience — Auto mode width sweep", "[ui][layout]") {
    ensureQApp();

    ColormapControls controls;
    controls.setColorRangeMode(ColorRangeConfig::Mode::Auto);
    controls.show();
    QCoreApplication::processEvents();

    // In Auto mode, spinboxes are hidden, so even narrow widths should be OK
    for (int w = 50; w <= 500; w += 25) {
        controls.resize(w, 300);
        QCoreApplication::processEvents();

        auto violations = significantViolations(&controls);
        if (!violations.empty()) {
            INFO("Width: " << w);
            for (auto const & v: violations) {
                INFO("  " << v.widgetPath.toStdString()
                          << " — " << v.description.toStdString()
                          << " (actual: " << v.actual.width() << "x" << v.actual.height()
                          << ", expected: " << v.expected.width() << "x" << v.expected.height()
                          << ")");
            }
            CHECK(violations.empty());
        }
    }
}

TEST_CASE("ColormapControls layout resilience — Manual mode width sweep", "[ui][layout]") {
    ensureQApp();

    ColormapControls controls;
    controls.setColorRangeMode(ColorRangeConfig::Mode::Manual);
    controls.show();
    QCoreApplication::processEvents();

    std::vector<std::string> all_violations_log;

    for (int w = 50; w <= 500; w += 25) {
        controls.resize(w, 400);
        QCoreApplication::processEvents();

        auto violations = significantViolations(&controls);
        for (auto const & v: violations) {
            std::string const entry = "w=" + std::to_string(w) + " " +
                                v.widgetPath.toStdString() + " — " +
                                v.description.toStdString() +
                                " (actual: " + std::to_string(v.actual.width()) +
                                "x" + std::to_string(v.actual.height()) +
                                ", expected: " + std::to_string(v.expected.width()) +
                                "x" + std::to_string(v.expected.height()) + ")";
            all_violations_log.push_back(entry);
        }
    }

    // Log all violations at once for diagnostic clarity
    for (auto const & entry: all_violations_log) {
        WARN(entry);
    }
    CHECK(all_violations_log.empty());
}

TEST_CASE("ColormapControls layout resilience — Symmetric mode width sweep", "[ui][layout]") {
    ensureQApp();

    ColormapControls controls;
    controls.setColorRangeMode(ColorRangeConfig::Mode::Symmetric);
    controls.show();
    QCoreApplication::processEvents();

    for (int w = 50; w <= 500; w += 25) {
        controls.resize(w, 300);
        QCoreApplication::processEvents();

        auto violations = significantViolations(&controls);
        if (!violations.empty()) {
            INFO("Width: " << w);
            for (auto const & v: violations) {
                INFO("  " << v.widgetPath.toStdString()
                          << " — " << v.description.toStdString());
            }
            CHECK(violations.empty());
        }
    }
}

TEST_CASE("ColormapControls layout resilience — mode transitions at constrained widths", "[ui][layout]") {
    ensureQApp();

    ColormapControls controls;
    controls.show();
    QCoreApplication::processEvents();

    std::vector<int> const widths = {50, 100, 150, 200, 300, 500};
    std::vector<ColorRangeConfig::Mode> const modes = {
            ColorRangeConfig::Mode::Auto,
            ColorRangeConfig::Mode::Manual,
            ColorRangeConfig::Mode::Symmetric,
    };

    for (int const w: widths) {
        controls.resize(w, 400);
        for (auto mode: modes) {
            controls.setColorRangeMode(mode);
            QCoreApplication::processEvents();

            auto violations = significantViolations(&controls);
            if (!violations.empty()) {
                INFO("Width: " << w << ", Mode: " << static_cast<int>(mode));
                for (auto const & v: violations) {
                    INFO("  " << v.widgetPath.toStdString()
                              << " — " << v.description.toStdString());
                }
                CHECK(violations.empty());
            }
        }
    }
}
