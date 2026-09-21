/**
 * @file PenToolOptions_Widget.cpp
 * @brief Options bar page for the Media Viewer Pen tool
 */

#include "PenToolOptions_Widget.hpp"
#include "ToolOptionsLayoutHelpers.hpp"
#include "ui_PenToolOptions_Widget.h"

#include "Core/MediaWidgetState.hpp"

#include <QComboBox>

#include <algorithm>
#include <string>
#include <vector>

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

[[nodiscard]] std::vector<std::string> collectEnabledLineKeys(MediaWidgetState const & state) {
    std::vector<std::string> keys;
    for (QString const & key: state.enabledFeatures(QStringLiteral("line"))) {
        keys.push_back(key.toStdString());
    }
    std::ranges::sort(keys);
    return keys;
}

}// namespace

PenToolOptions_Widget::PenToolOptions_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::PenToolOptions_Widget) {
    ui->setupUi(this);
    configureShrinkableToolOptionsPage(this);
    configureCompactInstructionLabel(ui->instruction_label);
    _populateAppendEndpointCombo();
    _rebuildPenTargetCombo();

    connect(ui->append_endpoint_combo, &QComboBox::currentIndexChanged,
            this, &PenToolOptions_Widget::_onAppendEndpointChanged);
    connect(ui->pen_target_combo, &QComboBox::currentIndexChanged,
            this, &PenToolOptions_Widget::_onPenTargetComboChanged);
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
    connect(_state, &MediaWidgetState::featureEnabledChanged,
            this, [this](QString const &, QString const &, bool) {
                _onEnabledFeaturesChanged();
            });
    connect(_state, &MediaWidgetState::displayOptionsChanged,
            this, [this](QString const &, QString const &) {
                _onEnabledFeaturesChanged();
            });
    connect(_state, &MediaWidgetState::displayOptionsRemoved,
            this, [this](QString const &, QString const &) {
                _onEnabledFeaturesChanged();
            });

    _rebuildPenTargetCombo();
    _syncFromState();
}

void PenToolOptions_Widget::_populateAppendEndpointCombo() {
    ui->append_endpoint_combo->clear();
    ui->append_endpoint_combo->addItem(tr("Tip"));
    ui->append_endpoint_combo->addItem(tr("Base"));
    ui->append_endpoint_combo->addItem(tr("Nearest endpoint"));
}

void PenToolOptions_Widget::_rebuildPenTargetCombo() {
    if (!ui->pen_target_combo) {
        return;
    }

    ui->pen_target_combo->blockSignals(true);
    ui->pen_target_combo->clear();
    ui->pen_target_combo->addItem(tr("Selected line"));

    if (_state) {
        for (std::string const & key: collectEnabledLineKeys(*_state)) {
            QString const label = tr("New line · %1").arg(QString::fromStdString(key));
            ui->pen_target_combo->addItem(label, QString::fromStdString(key));
        }
    }

    int const target_index = _indexForCurrentPenTarget();
    ui->pen_target_combo->setCurrentIndex(target_index);
    ui->pen_target_combo->blockSignals(false);

    if (_state &&
        _state->linePrefs().pen_target_mode == PenLineTargetMode::NewLine &&
        target_index == 0) {
        LineInteractionPrefs prefs = _state->linePrefs();
        prefs.pen_target_mode = PenLineTargetMode::SelectedLine;
        prefs.pen_new_line_key.clear();
        _state->setLinePrefs(prefs);
    }
}

int PenToolOptions_Widget::_indexForCurrentPenTarget() const {
    if (!_state || _state->linePrefs().pen_target_mode != PenLineTargetMode::NewLine) {
        return 0;
    }

    QString const target_key = QString::fromStdString(_state->linePrefs().pen_new_line_key);
    for (int i = 1; i < ui->pen_target_combo->count(); ++i) {
        if (ui->pen_target_combo->itemData(i).toString() == target_key) {
            return i;
        }
    }

    return 0;
}

void PenToolOptions_Widget::_onPenTargetComboChanged(int index) {
    if (_updating_from_state || !_state || !ui->pen_target_combo) {
        return;
    }

    LineInteractionPrefs prefs = _state->linePrefs();

    if (index <= 0) {
        prefs.pen_target_mode = PenLineTargetMode::SelectedLine;
        prefs.pen_new_line_key.clear();
    } else {
        prefs.pen_target_mode = PenLineTargetMode::NewLine;
        prefs.pen_new_line_key = ui->pen_target_combo->itemData(index).toString().toStdString();
    }

    _state->setLinePrefs(prefs);
}

void PenToolOptions_Widget::_onAppendEndpointChanged(int index) {
    if (_updating_from_state || !_state) {
        return;
    }

    LineInteractionPrefs prefs = _state->linePrefs();
    prefs.append_endpoint = appendEndpointForIndex(index);
    _state->setLinePrefs(prefs);
}

void PenToolOptions_Widget::_onEnabledFeaturesChanged() {
    _rebuildPenTargetCombo();
}

void PenToolOptions_Widget::_syncFromState() {
    if (!_state || !ui->append_endpoint_combo || !ui->pen_target_combo) {
        return;
    }

    _updating_from_state = true;
    ui->append_endpoint_combo->setCurrentIndex(
            indexForAppendEndpoint(_state->linePrefs().append_endpoint));

    int const target_index = _indexForCurrentPenTarget();
    if (ui->pen_target_combo->currentIndex() != target_index) {
        ui->pen_target_combo->setCurrentIndex(target_index);
    }

    _updating_from_state = false;
}
