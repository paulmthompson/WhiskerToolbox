#include "DataInspectorPropertiesWidget.hpp"
#include "ui_DataInspectorPropertiesWidget.h"

#include "DataInspectorState.hpp"
#include "DataInspectorViewWidget.hpp"
#include "DigitalIntervalSeries/DigitalIntervalSeriesDataView.hpp"
#include "DigitalIntervalSeries/DigitalIntervalSeriesInspector.hpp"
#include "LineData/LineInspector.hpp"
#include "LineData/LineTableView.hpp"
#include "PointData/PointInspector.hpp"
#include "PointData/PointTableView.hpp"
#include "Inspectors/BaseInspector.hpp"
#include "Inspectors/InspectorFactory.hpp"

#include "DataManager/DataManager.hpp"
#include "EditorState/SelectionContext.hpp"

#include <QLabel>
#include <QVBoxLayout>

DataInspectorPropertiesWidget::DataInspectorPropertiesWidget(
    std::shared_ptr<DataManager> data_manager,
    GroupManager * group_manager,
    QWidget * parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::DataInspectorPropertiesWidget>())
    , _data_manager(std::move(data_manager))
    , _group_manager(group_manager) {
    ui->setupUi(this);
    _setupUi();
    _connectSignals();
}

DataInspectorPropertiesWidget::~DataInspectorPropertiesWidget() = default;

void DataInspectorPropertiesWidget::setState(std::shared_ptr<DataInspectorState> state) {
    // Disconnect from old state
    if (_state) {
        disconnect(_state.get(), nullptr, this, nullptr);
    }

    _state = std::move(state);

    if (_state) {
        connect(_state.get(), &DataInspectorState::inspectedDataKeyChanged,
                this, &DataInspectorPropertiesWidget::_onInspectedKeyChanged);
        connect(_state.get(), &DataInspectorState::pinnedChanged,
                this, [this](bool pinned) {
                    ui->pinButton->setChecked(pinned);
                });
        connect(_state.get(), &DataInspectorState::stateChanged,
                this, &DataInspectorPropertiesWidget::_onStateChanged);

        // Initialize UI from state
        ui->pinButton->setChecked(_state->isPinned());
        _onInspectedKeyChanged(_state->inspectedDataKey());
    }
}

void DataInspectorPropertiesWidget::setSelectionContext(SelectionContext * context) {
    // Disconnect from old context
    if (_selection_context) {
        disconnect(_selection_context, nullptr, this, nullptr);
    }

    _selection_context = context;

    if (_selection_context) {
        connect(_selection_context, &SelectionContext::selectionChanged,
                this, &DataInspectorPropertiesWidget::_onSelectionChanged);
    }
}

void DataInspectorPropertiesWidget::inspectData(QString const & key) {
    if (_state) {
        _state->setInspectedDataKey(key);
    } else {
        _updateInspectorForKey(key);
    }
}

void DataInspectorPropertiesWidget::_setupUi() {
    // Set up the pin button
    ui->pinButton->setCheckable(true);
    ui->pinButton->setToolTip(tr("Pin to keep inspecting this data regardless of selection"));
    
    // Set up placeholder content
    _updateHeaderDisplay();
}

void DataInspectorPropertiesWidget::_connectSignals() {
    connect(ui->pinButton, &QPushButton::toggled,
            this, &DataInspectorPropertiesWidget::_onPinToggled);
}

void DataInspectorPropertiesWidget::_onSelectionChanged(SelectionSource const & source) {
    // Ignore if pinned
    if (_state && _state->isPinned()) {
        return;
    }

    // Ignore if change came from us
    if (_state && source.editor_instance_id.toString() == _state->getInstanceId()) {
        return;
    }

    // Get selected data key from context
    if (_selection_context) {
        auto const selected = _selection_context->primarySelectedData();
        if (selected.isValid()) {
            inspectData(selected.toString());
        }
    }
}

void DataInspectorPropertiesWidget::_onPinToggled(bool checked) {
    if (_state) {
        _state->setPinned(checked);
    }
}

void DataInspectorPropertiesWidget::_onInspectedKeyChanged(QString const & key) {
    _updateInspectorForKey(key);
}

void DataInspectorPropertiesWidget::_onStateChanged() {
    _updateHeaderDisplay();
}

void DataInspectorPropertiesWidget::_updateInspectorForKey(QString const & key) {
    std::string const key_std = key.toStdString();
    
    if (key_std == _current_key && _current_inspector) {
        return;  // Already showing this data
    }

    _current_key = key_std;
    _updateHeaderDisplay();

    // Clear existing inspector
    _clearInspector();

    if (key.isEmpty() || !_data_manager) {
        return;
    }

    // Check if data exists
    auto const data_variant = _data_manager->getDataVariant(key_std);
    if (!data_variant.has_value()) {
        ui->dataKeyLabel->setText(tr("Data not found: %1").arg(key));
        return;
    }

    // Get the data type
    auto const data_type = _data_manager->getType(key_std);
    
    // Update type label
    QString type_name = QString::fromStdString(convert_data_type_to_string(data_type));
    ui->dataTypeLabel->setText(type_name);

    // Create type-specific inspector using the factory
    _createInspectorForType(data_type);

    // Set the active key on the inspector
    if (_current_inspector) {
        _current_inspector->setActiveKey(key_std);
        // Reconnect to view after setting key (view might have changed)
        _connectInspectorToView();
    }
}

void DataInspectorPropertiesWidget::_createInspectorForType(DM_DataType type) {
    // Check if we already have the right inspector type
    if (_current_inspector && _current_type == type) {
        return;
    }

    // Clear any existing inspector
    _clearInspector();

    // Create the appropriate inspector using the factory
    _current_inspector = InspectorFactory::createInspector(
        type,
        _data_manager,
        _group_manager,
        this);

    if (_current_inspector) {
        _current_type = type;
        ui->contentLayout->addWidget(_current_inspector.get());

        // Connect the inspector's frameSelected signal
        connect(_current_inspector.get(), &BaseInspector::frameSelected,
                this, &DataInspectorPropertiesWidget::frameSelected);
        
        // Connect inspector to view if needed
        _connectInspectorToView();
    } else {
        // No inspector available for this type - show placeholder
        _current_type = DM_DataType::Unknown;
        auto * placeholder = new QLabel(
            tr("No inspector available for type: %1")
                .arg(QString::fromStdString(convert_data_type_to_string(type))),
            this);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setWordWrap(true);
        ui->contentLayout->addWidget(placeholder);
    }
}

void DataInspectorPropertiesWidget::_updateHeaderDisplay() {
    if (_current_key.empty()) {
        ui->dataKeyLabel->setText(tr("No data selected"));
        ui->dataTypeLabel->setText(QString());
    } else {
        ui->dataKeyLabel->setText(QString::fromStdString(_current_key));
    }
}

void DataInspectorPropertiesWidget::_clearInspector() {
    if (_current_inspector) {
        // Disconnect signals
        disconnect(_current_inspector.get(), nullptr, this, nullptr);
        
        // Remove callbacks from data
        _current_inspector->removeCallbacks();
        
        // Remove from layout and delete
        ui->contentLayout->removeWidget(_current_inspector.get());
        _current_inspector.reset();
        _current_type = DM_DataType::Unknown;
    }
}

void DataInspectorPropertiesWidget::setViewWidget(DataInspectorViewWidget * view_widget) {
    _view_widget = view_widget;
    _connectInspectorToView();
}

void DataInspectorPropertiesWidget::_connectInspectorToView() {
    if (!_current_inspector || !_view_widget) {
        return;
    }
    
    // Check if this is a DigitalIntervalSeriesInspector and connect it to the view
    auto * interval_inspector = dynamic_cast<DigitalIntervalSeriesInspector *>(_current_inspector.get());
    if (interval_inspector) {
        auto * interval_view = dynamic_cast<DigitalIntervalSeriesDataView *>(_view_widget->currentView());
        if (interval_view) {
            interval_inspector->setDataView(interval_view);
        }
    }

    // Check if this is a LineInspector and connect it to the view
    auto * line_inspector = dynamic_cast<LineInspector *>(_current_inspector.get());
    if (line_inspector) {
        auto * line_view = dynamic_cast<LineTableView *>(_view_widget->currentView());
        if (line_view) {
            line_inspector->setDataView(line_view);
        }
    }

    // Check if this is a PointInspector and connect it to the view
    auto * point_inspector = dynamic_cast<PointInspector *>(_current_inspector.get());
    if (point_inspector) {
        auto * point_view = dynamic_cast<PointTableView *>(_view_widget->currentView());
        if (point_view) {
            point_inspector->setTableView(point_view);
        }
    }
}
