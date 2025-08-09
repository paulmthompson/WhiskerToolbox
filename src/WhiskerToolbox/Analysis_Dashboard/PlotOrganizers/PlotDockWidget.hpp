#ifndef PLOTDOCKWIDGET_HPP
#define PLOTDOCKWIDGET_HPP

// Lakos include order
// 1) Prototype header
// 2) Project headers
// 3) Third-party
#include "DockWidget.h"
// 4) Almost-standard
// 5) Standard

#include <QString>

/**
 * @brief Dock widget wrapper for a plot, used to detect close events
 */
class PlotDockWidget : public ads::CDockWidget {
    Q_OBJECT

public:
    explicit PlotDockWidget(QString const& plot_id, QWidget* content, QWidget* parent = nullptr);

signals:
    void closeRequested(QString const& plot_id);

protected:
    void closeEvent(QCloseEvent* ev) override;

private:
    QString _plot_id;
};

#endif // PLOTDOCKWIDGET_HPP

