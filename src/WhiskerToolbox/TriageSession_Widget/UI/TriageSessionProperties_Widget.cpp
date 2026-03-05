/**
 * @file TriageSessionProperties_Widget.cpp
 * @brief Implementation of the Triage Session properties widget
 */

#include "TriageSessionProperties_Widget.hpp"

#include "Core/TriageSessionState.hpp"

#include <QLabel>
#include <QVBoxLayout>

TriageSessionProperties_Widget::TriageSessionProperties_Widget(
        std::shared_ptr<TriageSessionState> state,
        QWidget * parent)
    : QWidget(parent),
      _state(std::move(state)) {

    auto * layout = new QVBoxLayout(this);

    auto * placeholder = new QLabel(QStringLiteral("Triage Session — UI coming soon"), this);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);

    layout->addStretch();
}
