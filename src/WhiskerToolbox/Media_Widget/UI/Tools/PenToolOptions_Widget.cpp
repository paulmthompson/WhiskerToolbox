/**
 * @file PenToolOptions_Widget.cpp
 * @brief Options bar page for the Media Viewer Pen tool
 */

#include "PenToolOptions_Widget.hpp"
#include "ui_PenToolOptions_Widget.h"

#include "Core/MediaWidgetState.hpp"

#include <QComboBox>

namespace {

[[nodiscard]] int indexForAppendEndpoint(LineAppendEndpoint endpoint) {
    switch (endpoint) {
        case LineAppendEndpoint::Tip:
            return 0;
        case LineAppendEndpoint::Base:
            return 1;
        case LineAppendEndpoint::Nearest:
            return 2;
    }
    return 0;
}

[[nodiscard]] LineAppendEndpoint appendEndpointForIndex(int index) {
    switch (index) {
        case 1:
            return LineAppendEndpoint::Base;
        case 2:
            return LineAppendEndpoint::Nearest;
        case 0:
        default:
            return LineAppendEndpoint::Tip;
    }
}

}// namespace

PenToolOptions_Widget::PenToolOptions_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::PenToolOptions_Widget) {
    ui->setupUi(this);
    _populateAppendEndpointCombo();

    connect(ui->append_endpoint_combo, &QComboBox::currentIndexChanged,
            this, &PenToolOptions_Widget::_onAppendEndpointChanged);
}

PenToolOptions_Widget::~PenToolOptions_Widget() {
    delete ui;
}

void PenToolOptions_Widget::setState(MediaWidgetState * state) {
    if (_state) {
        disconnect(_state, nullptr, this, nullptr);
    }

    _state = state;

    if (!_state) {
        return;
    }

    connect(_state, &MediaWidgetState::interactionPrefsChanged,
            this, [this](QString const & data_type) {
                if (data_type == QStringLiteral("line")) {
                    _syncFromState();
                }
            });

    _syncFromState();
}

void PenToolOptions_Widget::_populateAppendEndpointCombo() {
    ui->append_endpoint_combo->clear();
    ui->append_endpoint_combo->addItem(tr("Tip"));
    ui->append_endpoint_combo->addItem(tr("Base"));
    ui->append_endpoint_combo->addItem(tr("Nearest endpoint"));
}

void PenToolOptions_Widget::_onAppendEndpointChanged(int index) {
    if (_updating_from_state || !_state) {
        return;
    }

    LineInteractionPrefs prefs = _state->linePrefs();
    prefs.append_endpoint = appendEndpointForIndex(index);
    _state->setLinePrefs(prefs);
}

void PenToolOptions_Widget::_syncFromState() {
    if (!_state || !ui->append_endpoint_combo) {
        return;
    }

    _updating_from_state = true;
    ui->append_endpoint_combo->setCurrentIndex(
            indexForAppendEndpoint(_state->linePrefs().append_endpoint));
    _updating_from_state = false;
}
