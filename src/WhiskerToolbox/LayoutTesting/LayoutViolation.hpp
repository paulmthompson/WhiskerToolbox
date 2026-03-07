/**
 * @file LayoutViolation.hpp
 * @brief Structured violation report for layout sanity checking
 *
 * Describes a single layout constraint violation detected by LayoutSanityChecker,
 * including severity, the widget's ancestry path, and actual vs. expected sizes.
 */

#ifndef LAYOUT_VIOLATION_HPP
#define LAYOUT_VIOLATION_HPP

#include <QSize>
#include <QString>

/**
 * @brief A single layout constraint violation detected during a widget tree walk
 */
struct LayoutViolation {
    /**
     * @brief Severity of the violation
     */
    enum class Severity {
        Critical,///< Widget is completely unusable (e.g. zero dimension)
        Warning, ///< Widget is below recommended minimum size
        Info,    ///< Informational anomaly (e.g. extreme aspect ratio)
    };

    Severity severity;
    QString widgetPath; ///< Ancestry path, e.g. "MainWindow/DockWidget/ColormapControls/vmax_spin"
    QString className;  ///< Meta-object class name, e.g. "QDoubleSpinBox"
    QString description;///< Human-readable explanation of the violation
    QSize actual;       ///< Actual widget size
    QSize expected;     ///< minimumSizeHint or computed minimum

    /// @brief Equality comparison for violation diffing (compares path + description)
    [[nodiscard]] bool operator==(LayoutViolation const & other) const {
        return severity == other.severity &&
               widgetPath == other.widgetPath &&
               description == other.description;
    }
};

#endif// LAYOUT_VIOLATION_HPP
