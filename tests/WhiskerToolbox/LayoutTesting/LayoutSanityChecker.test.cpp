/**
 * @file LayoutSanityChecker.test.cpp
 * @brief Unit tests for the LayoutSanityChecker
 *
 * Verifies that the checker correctly detects zero-dimension widgets,
 * minimumSizeHint violations, and extreme aspect ratios, and that it
 * produces no false positives for healthy layouts.
 */

#include "LayoutTesting/LayoutSanityChecker.hpp"

#include <QApplication>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>

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

// =============================================================================
// Helper: filter violations by severity
// =============================================================================
static auto countBySeverity(std::vector<LayoutViolation> const & violations,
                            LayoutViolation::Severity severity) -> std::size_t {
    return static_cast<std::size_t>(
            std::count_if(violations.begin(), violations.end(),
                          [severity](auto const & v) { return v.severity == severity; }));
}

// =============================================================================
// Check 1: Zero dimension
// =============================================================================

TEST_CASE("LayoutSanityChecker detects zero-width widget", "[ui][layout]") {
    ensureQApp();

    QWidget parent;
    auto * child = new QLabel("Hello", &parent);
    child->setFixedSize(0, 50);
    parent.show();
    child->show();
    QCoreApplication::processEvents();

    auto violations = LayoutSanityChecker::getViolations(&parent);
    auto critical = countBySeverity(violations, LayoutViolation::Severity::Critical);
    REQUIRE(critical >= 1);

    // The zero-dimension violation should reference the child
    bool const found = std::any_of(violations.begin(), violations.end(), [](auto const & v) {
        return v.severity == LayoutViolation::Severity::Critical &&
               v.description.contains("zero dimension");
    });
    REQUIRE(found);
}

TEST_CASE("LayoutSanityChecker detects zero-height widget", "[ui][layout]") {
    ensureQApp();

    QWidget parent;
    auto * child = new QLabel("Hello", &parent);
    child->setFixedSize(50, 0);
    parent.show();
    child->show();
    QCoreApplication::processEvents();

    auto violations = LayoutSanityChecker::getViolations(&parent);
    bool const found = std::any_of(violations.begin(), violations.end(), [](auto const & v) {
        return v.severity == LayoutViolation::Severity::Critical &&
               v.description.contains("zero dimension");
    });
    REQUIRE(found);
}

// =============================================================================
// Check 2: Below minimumSizeHint
// =============================================================================

TEST_CASE("LayoutSanityChecker detects widget below minimumSizeHint", "[ui][layout]") {
    ensureQApp();

    QWidget parent;
    auto * spin = new QDoubleSpinBox(&parent);
    spin->setDecimals(4);
    spin->setRange(-1e9, 1e9);

    parent.show();
    spin->show();
    QCoreApplication::processEvents();

    // Force the spinbox to be much smaller than its minimumSizeHint
    QSize const minHint = spin->minimumSizeHint();
    REQUIRE(minHint.width() > 10);// Sanity: spin has a real min hint
    spin->setFixedSize(5, 5);
    QCoreApplication::processEvents();

    auto violations = LayoutSanityChecker::getViolations(&parent);
    bool const found = std::any_of(violations.begin(), violations.end(), [](auto const & v) {
        return v.severity == LayoutViolation::Severity::Warning &&
               v.description.contains("minimumSizeHint");
    });
    REQUIRE(found);
}

// =============================================================================
// Check 3: Extreme aspect ratio
// =============================================================================

TEST_CASE("LayoutSanityChecker detects extreme aspect ratio", "[ui][layout]") {
    ensureQApp();

    QWidget parent;
    auto * child = new QWidget(&parent);
    // 500:10 = 50:1 ratio, well above the 20:1 threshold
    child->setFixedSize(500, 10);
    parent.show();
    child->show();
    QCoreApplication::processEvents();

    auto violations = LayoutSanityChecker::getViolations(&parent);
    bool const found = std::any_of(violations.begin(), violations.end(), [](auto const & v) {
        return v.severity == LayoutViolation::Severity::Info &&
               v.description.contains("aspect ratio");
    });
    REQUIRE(found);
}

// =============================================================================
// Healthy layout — no false positives
// =============================================================================

TEST_CASE("LayoutSanityChecker produces no violations for healthy layout", "[ui][layout]") {
    ensureQApp();

    QWidget parent;
    auto * layout = new QVBoxLayout(&parent);
    layout->addWidget(new QLabel("Label", &parent));
    layout->addWidget(new QPushButton("Button", &parent));
    parent.resize(300, 200);
    parent.show();
    QCoreApplication::processEvents();

    auto violations = LayoutSanityChecker::getViolations(&parent);

    // Filter out any violations from internal Qt widgets we don't control
    std::erase_if(violations, [](auto const & v) {
        return v.severity == LayoutViolation::Severity::Info;
    });

    REQUIRE(violations.empty());
}

// =============================================================================
// Widget path
// =============================================================================

TEST_CASE("LayoutSanityChecker widgetPath includes object names", "[ui][layout]") {
    ensureQApp();

    QWidget parent;
    parent.setObjectName("MyParent");
    auto * child = new QWidget(&parent);
    child->setObjectName("MyChild");

    QString const path = LayoutSanityChecker::widgetPath(child);
    REQUIRE(path.contains("MyParent"));
    REQUIRE(path.contains("MyChild"));
    REQUIRE(path == "MyParent/MyChild");
}

TEST_CASE("LayoutSanityChecker widgetPath falls back to class name", "[ui][layout]") {
    ensureQApp();

    QWidget parent;
    auto * child = new QPushButton("Click", &parent);
    // No objectName set

    QString const path = LayoutSanityChecker::widgetPath(child);
    REQUIRE(path.contains("QWidget"));
    REQUIRE(path.contains("QPushButton"));
}

// =============================================================================
// Invisible widgets are skipped
// =============================================================================

TEST_CASE("LayoutSanityChecker skips invisible widgets", "[ui][layout]") {
    ensureQApp();

    QWidget parent;
    auto * child = new QWidget(&parent);
    child->setFixedSize(0, 0);// Would be Critical if visible
    child->hide();

    parent.show();
    QCoreApplication::processEvents();

    auto violations = LayoutSanityChecker::getViolations(&parent);
    // The hidden zero-dimension child should not produce violations
    bool const found = std::any_of(violations.begin(), violations.end(), [](auto const & v) {
        return v.severity == LayoutViolation::Severity::Critical;
    });
    REQUIRE_FALSE(found);
}
