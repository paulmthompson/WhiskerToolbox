/**
 * @file DataSynthesizerProperties_Widget.cpp
 * @brief Implementation of the DataSynthesizer properties panel
 */

#include "DataSynthesizerProperties_Widget.hpp"

#include <QLabel>
#include <QVBoxLayout>

DataSynthesizerProperties_Widget::DataSynthesizerProperties_Widget(
        std::shared_ptr<DataSynthesizerState> state,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)) {
    auto * layout = new QVBoxLayout(this);

    auto * label = new QLabel(QStringLiteral("Data Synthesizer \u2014 Properties"), this);
    label->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);

    layout->addStretch();
}
