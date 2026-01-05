#ifndef PLOTDOCKWIDGET_HPP
#define PLOTDOCKWIDGET_HPP

#include "DockWidget.h"

#include <QString>

/**
 * @brief Dock widget wrapper for a plot, used to detect close events
 */
class PlotDockWidget : public ads::CDockWidget {
    Q_OBJECT

public:
    explicit PlotDockWidget(QString const & plot_id, QWidget * content, QWidget * parent = nullptr);

signals:
    void closeRequested(QString const & plot_id);

protected:
    void closeEvent(QCloseEvent * ev) override;

private:
    QString _plot_id;
};

#endif// PLOTDOCKWIDGET_HPP
