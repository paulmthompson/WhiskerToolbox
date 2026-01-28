#include "DataInspectorViewWidget.hpp"
#include "ui_DataInspectorViewWidget.h"

#include "DataInspectorState.hpp"
#include "Inspectors/BaseDataView.hpp"
#include "Inspectors/ViewFactory.hpp"

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

DataInspectorViewWidget::~DataInspectorViewWidget() {
    _clearView();
}

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
    
    if (key_std == _current_key && _current_data_view) {
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
        _placeholder_widget = label;
        return;
    }

    // Get the data type
    auto const data_type = _data_manager->getType(key_std);
    
    // Try to create a type-specific view using the factory
    _createViewForType(data_type);

    // Set the active key on the view
    if (_current_data_view) {
        _current_data_view->setActiveKey(key_std);
    }
}

void DataInspectorViewWidget::_createViewForType(DM_DataType type) {
    // Check if we already have the right view type
    if (_current_data_view && _current_type == type) {
        return;
    }

    // Clear any existing view
    _clearView();

    // Create the appropriate view using the factory
    _current_data_view = ViewFactory::createView(type, _data_manager, this);

    if (_current_data_view) {
        _current_type = type;
        ui->contentLayout->addWidget(_current_data_view.get());

        // Connect the view's frameSelected signal
        connect(_current_data_view.get(), &BaseDataView::frameSelected,
                this, &DataInspectorViewWidget::frameSelected);
    } else {
        // No view available for this type - show placeholder
        _current_type = DM_DataType::Unknown;
        QString type_name = QString::fromStdString(convert_data_type_to_string(type));
        
        auto * placeholder = new QLabel(
            tr("No table view available for type: %1\n\n"
               "Use the Properties panel on the right for data inspection.")
                .arg(type_name),
            this);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setWordWrap(true);
        placeholder->setStyleSheet("color: gray; padding: 20px;");
        ui->contentLayout->addWidget(placeholder);
        _placeholder_widget = placeholder;
    }
}

void DataInspectorViewWidget::_clearView() {
    if (_current_data_view) {
        // Disconnect signals
        disconnect(_current_data_view.get(), nullptr, this, nullptr);
        
        // Remove callbacks from data
        _current_data_view->removeCallbacks();
        
        // Remove from layout and delete
        ui->contentLayout->removeWidget(_current_data_view.get());
        _current_data_view.reset();
        _current_type = DM_DataType::Unknown;
    }

    if (_placeholder_widget) {
        ui->contentLayout->removeWidget(_placeholder_widget);
        _placeholder_widget->deleteLater();
        _placeholder_widget = nullptr;
    }
}
