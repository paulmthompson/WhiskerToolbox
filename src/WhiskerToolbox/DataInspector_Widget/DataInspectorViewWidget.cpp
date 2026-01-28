#include "DataInspectorViewWidget.hpp"
#include "ui_DataInspectorViewWidget.h"

#include "DataInspectorState.hpp"

#include "DataManager/DataManager.hpp"

#include <QLabel>
#include <QVBoxLayout>

DataInspectorViewWidget::DataInspectorViewWidget(
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::DataInspectorViewWidget>())
    , _data_manager(std::move(data_manager)) {
    ui->setupUi(this);
    _setupUi();
}

DataInspectorViewWidget::~DataInspectorViewWidget() = default;

void DataInspectorViewWidget::setState(std::shared_ptr<DataInspectorState> state) {
    // Disconnect from old state
    if (_state) {
        disconnect(_state.get(), nullptr, this, nullptr);
    }

    _state = std::move(state);

    if (_state) {
        connect(_state.get(), &DataInspectorState::inspectedDataKeyChanged,
                this, &DataInspectorViewWidget::_onInspectedKeyChanged);

        // Initialize from state
        _onInspectedKeyChanged(_state->inspectedDataKey());
    }
}

void DataInspectorViewWidget::_setupUi() {
    // Initial placeholder is set in the .ui file
}

void DataInspectorViewWidget::_onInspectedKeyChanged(QString const & key) {
    _updateViewForKey(key);
}

void DataInspectorViewWidget::_updateViewForKey(QString const & key) {
    std::string const key_std = key.toStdString();
    
    if (key_std == _current_key) {
        return;  // Already showing this data
    }

    _current_key = key_std;
    _clearView();

    if (key.isEmpty() || !_data_manager) {
        // Show default placeholder
        ui->placeholderLabel->setVisible(true);
        return;
    }

    // Hide the default placeholder when we have data
    ui->placeholderLabel->setVisible(false);

    // Check if data exists
    auto const data_variant = _data_manager->getDataVariant(key_std);
    if (!data_variant.has_value()) {
        auto * label = new QLabel(tr("Data not found: %1").arg(key), this);
        label->setAlignment(Qt::AlignCenter);
        ui->contentLayout->addWidget(label);
        _current_view = label;
        return;
    }

    // Get the data type
    auto const data_type = _data_manager->getType(key_std);
    QString type_name = QString::fromStdString(convert_data_type_to_string(data_type));

    // Phase 1: Show placeholder for data view
    // Phase 3 will add type-specific views (tables, visualizations)
    auto * placeholder = new QLabel(
        tr("Data View: %1\n\nType: %2\n\n"
           "Data tables and visualizations will be added in Phase 3.\n\n"
           "For now, use the Properties panel on the right for data inspection.")
            .arg(key)
            .arg(type_name),
        this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setWordWrap(true);
    placeholder->setStyleSheet("color: gray; padding: 20px;");
    
    ui->contentLayout->addWidget(placeholder);
    _current_view = placeholder;
}

void DataInspectorViewWidget::_clearView() {
    if (_current_view) {
        ui->contentLayout->removeWidget(_current_view);
        _current_view->deleteLater();
        _current_view = nullptr;
    }
}
