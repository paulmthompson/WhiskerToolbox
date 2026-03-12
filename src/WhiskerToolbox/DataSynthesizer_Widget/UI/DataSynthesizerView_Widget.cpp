/**
 * @file DataSynthesizerView_Widget.cpp
 * @brief Implementation of the DataSynthesizer view/preview panel
 */

#include "DataSynthesizerView_Widget.hpp"

#include <QLabel>
#include <QVBoxLayout>

DataSynthesizerView_Widget::DataSynthesizerView_Widget(
        std::shared_ptr<DataSynthesizerState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)) {
    auto * layout = new QVBoxLayout(this);

    auto * label = new QLabel(QStringLiteral("Data Synthesizer \u2014 Preview"), this);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    layout->addStretch();
}
