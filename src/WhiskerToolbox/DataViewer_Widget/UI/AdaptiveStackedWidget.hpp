#ifndef DATAVIEWER_WIDGET_UI_ADAPTIVESTACKEDWIDGET_HPP
#define DATAVIEWER_WIDGET_UI_ADAPTIVESTACKEDWIDGET_HPP

#include <QStackedWidget>

class QShowEvent;

/**
 * @file AdaptiveStackedWidget.hpp
 * @brief QStackedWidget whose size hints follow the *current* page only.
 *
 * The default QStackedWidget reports the maximum of all pages' minimum sizes, which forces
 * the Feature Controls panel to reserve vertical space for the tallest editor (e.g. analog)
 * even when a compact editor (events/intervals) is visible. This widget delegates @c sizeHint
 * and @c minimumSizeHint to @c currentWidget() so the panel height tracks the active page.
 */
class AdaptiveStackedWidget final : public QStackedWidget {
public:
    explicit AdaptiveStackedWidget(QWidget * parent = nullptr);

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void showEvent(QShowEvent * event) override;

private:
    /// @brief Invalidate geometry up through the properties scroll content.
    void _propagateGeometryUpdate();
};

#endif// DATAVIEWER_WIDGET_UI_ADAPTIVESTACKEDWIDGET_HPP
