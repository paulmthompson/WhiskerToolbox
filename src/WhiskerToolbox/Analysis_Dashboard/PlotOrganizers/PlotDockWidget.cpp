#include "PlotDockWidget.hpp"

#include <QCloseEvent>

PlotDockWidget::PlotDockWidget(QString const& plot_id, QWidget* content, QWidget* parent)
    : ads::CDockWidget(plot_id, parent)
    , _plot_id(plot_id)
{
    setWidget(content);
}

void PlotDockWidget::closeEvent(QCloseEvent* ev)
{
    emit closeRequested(_plot_id);
    ads::CDockWidget::closeEvent(ev);
}

