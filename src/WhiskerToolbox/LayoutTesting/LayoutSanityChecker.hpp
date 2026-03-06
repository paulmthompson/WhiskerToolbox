/**
 * @file LayoutSanityChecker.hpp
 * @brief Runtime layout constraint checker for Qt widget trees
 *
 * Provides both a global event-filter monitor (for development-time awareness)
 * and a static getViolations() API for use in Catch2 tests. Detects widgets
 * that are zero-sized, smaller than their minimumSizeHint, or squished to
 * extreme aspect ratios.
 */

#ifndef LAYOUT_SANITY_CHECKER_HPP
#define LAYOUT_SANITY_CHECKER_HPP

#include "LayoutViolation.hpp"

#include <QObject>
#include <QTimer>

#include <vector>

class QWidget;

/**
 * @brief Runtime layout constraint checker for Qt widget trees
 *
 * Two usage modes:
 *
 * 1. **Global monitor** — construct an instance (typically in main.cpp under
 *    `#ifndef NDEBUG`). It installs itself as an event filter on `qApp` and
 *    logs violations via `qWarning` whenever layouts settle after a resize or
 *    layout request on a top-level window.
 *
 * 2. **Static query** — call `getViolations(root)` from a Catch2 test to
 *    collect all violations under a widget subtree. No event filter needed.
 */
class LayoutSanityChecker : public QObject {
    Q_OBJECT

public:
    explicit LayoutSanityChecker(QObject * parent = nullptr);
    ~LayoutSanityChecker() override = default;

    /**
     * @brief Collect all layout violations under @p root (including root itself)
     *
     * Only visible widgets are checked. Invisible widgets and their children
     * are skipped.
     *
     * @param root  Widget subtree to inspect
     * @return Vector of violations found (empty if layout is healthy)
     */
    [[nodiscard]] static auto getViolations(QWidget * root) -> std::vector<LayoutViolation>;

    /**
     * @brief Build a diagnostic ancestry path for a widget
     *
     * Walks up the parent chain, using objectName if set or the meta-object
     * class name otherwise. Example: "MainWindow/DockWidget/ColormapControls/vmax_spin"
     */
    [[nodiscard]] static auto widgetPath(QWidget const * widget) -> QString;

protected:
    bool eventFilter(QObject * obj, QEvent * event) override;

private:
    void scheduleCheck();
    static void runCheck();
    static void walkTree(QWidget * widget, std::vector<LayoutViolation> & out);

    QTimer _debounce_timer;
};

#endif// LAYOUT_SANITY_CHECKER_HPP
