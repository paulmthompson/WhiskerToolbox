/**
 * @file DataSynthesizerView_Widget.cpp
 * @brief Implementation of the DataSynthesizer view/preview panel
 */

#include "DataSynthesizerView_Widget.hpp"

#include "SynthesizerPreviewWidget.hpp"

#include <QVBoxLayout>

DataSynthesizerView_Widget::DataSynthesizerView_Widget(
        std::shared_ptr<DataSynthesizerState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)) {
    auto * layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    _preview_widget = new SynthesizerPreviewWidget(this);
    layout->addWidget(_preview_widget);
}

void DataSynthesizerView_Widget::setPreviewData(std::shared_ptr<AnalogTimeSeries> series) {
    if (_preview_widget) {
        if (series) {
            _preview_widget->setData(std::move(series));
        } else {
            _preview_widget->clearData();
        }
    }
}
