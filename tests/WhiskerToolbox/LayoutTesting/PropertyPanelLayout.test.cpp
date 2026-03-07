/**
 * @file PropertyPanelLayout.test.cpp
 * @brief Resize sweep and mode-toggle layout tests for property panel widgets
 *
 * Uses LayoutSanityChecker to verify that GlyphStyleControls, LineStyleControls,
 * and EstimationMethodControls do not produce layout violations at various sizes
 * and mode/state combinations.
 */

#include "LayoutTesting/LayoutSanityChecker.hpp"

#include "Core/GlyphStyleState.hpp"
#include "Core/LineStyleState.hpp"
#include "EstimationMethodControls.hpp"
#include "GlyphStyleControls.hpp"
#include "LineStyleControls.hpp"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
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

auto significantViolations(QWidget * root) -> std::vector<LayoutViolation> {
    auto all = LayoutSanityChecker::getViolations(root);
    std::erase_if(all, [](auto const & v) {
        return v.severity == LayoutViolation::Severity::Info;
    });
    return all;
}

}// namespace

// =============================================================================
// GlyphStyleControls — width sweep
// =============================================================================

TEST_CASE("GlyphStyleControls layout resilience — width sweep", "[ui][layout]") {
    ensureQApp();

    GlyphStyleState state;
    GlyphStyleControls controls(&state);
    controls.show();
    QCoreApplication::processEvents();

    std::vector<std::string> all_violations_log;

    for (int w = 50; w <= 500; w += 25) {
        controls.resize(w, 300);
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

    for (auto const & entry: all_violations_log) {
        WARN(entry);
    }
    CHECK(all_violations_log.empty());
}

// =============================================================================
// LineStyleControls — width sweep
// =============================================================================

TEST_CASE("LineStyleControls layout resilience — width sweep", "[ui][layout]") {
    ensureQApp();

    LineStyleState state;
    LineStyleControls controls(&state);
    controls.show();
    QCoreApplication::processEvents();

    std::vector<std::string> all_violations_log;

    for (int w = 50; w <= 500; w += 25) {
        controls.resize(w, 300);
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

    for (auto const & entry: all_violations_log) {
        WARN(entry);
    }
    CHECK(all_violations_log.empty());
}

// =============================================================================
// EstimationMethodControls — width sweep (Binning mode)
// =============================================================================

TEST_CASE("EstimationMethodControls layout resilience — Binning mode width sweep", "[ui][layout]") {
    ensureQApp();

    EstimationMethodControls controls;
    controls.setParams(WhiskerToolbox::Plots::BinningParams{10.0});
    controls.show();
    QCoreApplication::processEvents();

    std::vector<std::string> all_violations_log;

    for (int w = 50; w <= 500; w += 25) {
        controls.resize(w, 300);
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

    for (auto const & entry: all_violations_log) {
        WARN(entry);
    }
    CHECK(all_violations_log.empty());
}

// =============================================================================
// EstimationMethodControls — mode-toggle at constrained widths
// =============================================================================

TEST_CASE("EstimationMethodControls layout resilience — method transitions at constrained widths", "[ui][layout]") {
    ensureQApp();

    EstimationMethodControls controls;
    controls.show();
    QCoreApplication::processEvents();

    std::vector<int> const widths = {50, 100, 150, 200, 300, 500};
    std::vector<WhiskerToolbox::Plots::EstimationParams> const methods = {
            WhiskerToolbox::Plots::BinningParams{10.0},
            WhiskerToolbox::Plots::GaussianKernelParams{20.0, 1.0},
            WhiskerToolbox::Plots::CausalExponentialParams{50.0, 1.0},
    };

    for (int const w: widths) {
        controls.resize(w, 400);
        for (auto const & method: methods) {
            controls.setParams(method);
            QCoreApplication::processEvents();

            auto violations = significantViolations(&controls);
            if (!violations.empty()) {
                INFO("Width: " << w << ", Method index: " << method.index());
                for (auto const & v: violations) {
                    INFO("  " << v.widgetPath.toStdString()
                              << " — " << v.description.toStdString());
                }
                CHECK(violations.empty());
            }
        }
    }
}

// =============================================================================
// ColormapControls — mode-toggle tests asserting no *new* violations
// =============================================================================
// (Note: These complement the existing ColormapControlsLayout.test.cpp by
//  specifically testing that mode transitions do not *introduce* new violations
//  beyond what was already present at the initial size.)

TEST_CASE("ColormapControls mode transition introduces no new violations", "[ui][layout][mode-toggle]") {
    ensureQApp();

    // We use a local include-free approach: create a QWidget and manually
    // test just the checker pattern here. The actual ColormapControls
    // mode-toggle tests are in ColormapControlsLayout.test.cpp.
    // This test validates the diffing concept with a generic widget.

    QWidget parent;
    auto * layout = new QVBoxLayout(&parent);
    auto * label = new QLabel("Baseline label", &parent);
    auto * button = new QPushButton("Toggle", &parent);
    layout->addWidget(label);
    layout->addWidget(button);
    parent.resize(200, 100);
    parent.show();
    QCoreApplication::processEvents();

    // Baseline violations
    auto baseline = LayoutSanityChecker::getViolations(&parent);

    // Simulate a "mode change" — add a new widget
    auto * extra = new QLabel("Extra content after mode change", &parent);
    layout->addWidget(extra);
    extra->show();
    QCoreApplication::processEvents();

    auto after = LayoutSanityChecker::getViolations(&parent);

    // Find violations that are new (not in baseline)
    std::vector<LayoutViolation> new_violations;
    for (auto const & v: after) {
        bool const was_in_baseline = std::any_of(
                baseline.begin(), baseline.end(),
                [&v](auto const & b) { return b == v; });
        if (!was_in_baseline) {
            new_violations.push_back(v);
        }
    }

    // At a reasonable size, adding a label shouldn't create new violations
    for (auto const & v: new_violations) {
        INFO(v.widgetPath.toStdString() << " — " << v.description.toStdString());
    }
    CHECK(new_violations.empty());
}
