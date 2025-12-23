#include "DataViewerTooltipController.hpp"

#include <QToolTip>
#include <QWidget>

namespace DataViewer {

DataViewerTooltipController::DataViewerTooltipController(QWidget * parent_widget)
    : QObject(parent_widget)
    , _parent_widget(parent_widget) {
    _timer = new QTimer(this);
    _timer->setSingleShot(true);
    _timer->setInterval(DEFAULT_DELAY_MS);
    connect(_timer, &QTimer::timeout, this, &DataViewerTooltipController::showTooltip);
}

void DataViewerTooltipController::setDelay(int delay_ms) {
    _timer->setInterval(delay_ms);
}

int DataViewerTooltipController::delay() const {
    return _timer->interval();
}

void DataViewerTooltipController::scheduleTooltip(QPoint const & canvas_pos) {
    _hover_pos = canvas_pos;
    _timer->start();// Restart timer with new position
}

void DataViewerTooltipController::cancel() {
    _timer->stop();
    QToolTip::hideText();
    emit tooltipHidden();
}

bool DataViewerTooltipController::isScheduled() const {
    return _timer->isActive();
}

void DataViewerTooltipController::setSeriesInfoProvider(SeriesInfoProvider provider) {
    _info_provider = std::move(provider);
}

void DataViewerTooltipController::showTooltip() {
    if (!_info_provider || !_parent_widget) {
        return;
    }

    float const canvas_x = static_cast<float>(_hover_pos.x());
    float const canvas_y = static_cast<float>(_hover_pos.y());

    auto series_info = _info_provider(canvas_x, canvas_y);

    if (series_info.has_value()) {
        SeriesInfo const & info = series_info.value();
        QString const tooltip_text = formatTooltipText(info);

        // Show tooltip at cursor position (global coordinates)
        QPoint const global_pos = _parent_widget->mapToGlobal(_hover_pos);
        QToolTip::showText(global_pos, tooltip_text, _parent_widget);

        emit tooltipShowing(_hover_pos, info);
    } else {
        // No series under cursor
        QToolTip::hideText();
        emit tooltipHidden();
    }
}

QString DataViewerTooltipController::formatTooltipText(SeriesInfo const & info) {
    QString tooltip_text;

    if (info.type == "Analog") {
        tooltip_text = QString("<b>Analog Series</b><br>Key: %1")
                               .arg(QString::fromStdString(info.key));
        if (info.has_value) {
            tooltip_text += QString("<br>Value: %1").arg(info.value, 0, 'f', 3);
        }
    } else if (info.type == "Event") {
        tooltip_text = QString("<b>Event Series</b><br>Key: %1")
                               .arg(QString::fromStdString(info.key));
    } else if (info.type == "Interval") {
        tooltip_text = QString("<b>Interval Series</b><br>Key: %1")
                               .arg(QString::fromStdString(info.key));
    } else {
        tooltip_text = QString("<b>%1 Series</b><br>Key: %2")
                               .arg(QString::fromStdString(info.type))
                               .arg(QString::fromStdString(info.key));
    }

    return tooltip_text;
}

}// namespace DataViewer
