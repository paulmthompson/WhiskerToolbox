#include "DataImport_Widget.hpp"
#include "ui_DataImport_Widget.h"

#include "DataImportTypeRegistry.hpp"
#include "EditorState/SelectionContext.hpp"

#include <QComboBox>
#include <QLabel>
#include <QStackedWidget>

DataImport_Widget::DataImport_Widget(
    std::shared_ptr<DataImportWidgetState> state,
    std::shared_ptr<DataManager> data_manager,
    SelectionContext * selection_context,
    QWidget * parent)
    : QWidget(parent),
      ui(new Ui::DataImport_Widget),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)),
      _selection_context(selection_context) {
    ui->setupUi(this);

    // Connect to SelectionContext for passive awareness
    connectToSelectionContext(_selection_context, this);

    // Set up data type combo box
    _setupDataTypeCombo();

    // Connect combo box to handler
    connect(ui->dataTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &DataImport_Widget::_onDataTypeComboChanged);

    // Connect to state changes
    connect(_state.get(), &DataImportWidgetState::selectedImportTypeChanged,
            this, &DataImport_Widget::_onStateTypeChanged);

    // Initialize from state if there's a previously selected type
    QString const initial_type = _state->selectedImportType();
    if (!initial_type.isEmpty()) {
        _switchToDataType(initial_type);
    } else {
        _updateHeader(QString());
    }
}

DataImport_Widget::~DataImport_Widget() {
    delete ui;
}

void DataImport_Widget::onDataFocusChanged(
    EditorLib::SelectedDataKey const & /* data_key */,
    QString const & data_type) {
    // Update state (this will trigger _onStateTypeChanged if different)
    _state->setSelectedImportType(data_type);
}

void DataImport_Widget::_onDataTypeComboChanged(int index) {
    if (index < 0) {
        return;
    }

    QString const data_type = ui->dataTypeCombo->itemData(index).toString();
    if (!data_type.isEmpty()) {
        _state->setSelectedImportType(data_type);
    }
}

void DataImport_Widget::_onStateTypeChanged(QString const & type) {
    _switchToDataType(type);

    // Update combo box to match (without triggering signal)
    ui->dataTypeCombo->blockSignals(true);
    int const index = ui->dataTypeCombo->findData(type);
    if (index >= 0) {
        ui->dataTypeCombo->setCurrentIndex(index);
    }
    ui->dataTypeCombo->blockSignals(false);
}

void DataImport_Widget::_setupDataTypeCombo() {
    ui->dataTypeCombo->clear();

    // Add a "Select type..." placeholder
    ui->dataTypeCombo->addItem(tr("Select data type..."), QString());

    // Add registered types from registry
    auto const & registry = DataImportTypeRegistry::instance();
    QStringList const types = registry.supportedTypes();

    for (QString const & type : types) {
        QString const display_name = registry.displayName(type);
        ui->dataTypeCombo->addItem(display_name.isEmpty() ? type : display_name, type);
    }
}

void DataImport_Widget::_switchToDataType(QString const & data_type) {
    _updateHeader(data_type);

    if (data_type.isEmpty()) {
        // Show placeholder/empty state
        ui->stackedWidget->setCurrentWidget(ui->emptyPage);
        return;
    }

    QWidget * widget = _getOrCreateWidgetForType(data_type);
    if (widget) {
        ui->stackedWidget->setCurrentWidget(widget);
    } else {
        // Type not registered - show placeholder
        ui->stackedWidget->setCurrentWidget(ui->emptyPage);
        ui->emptyLabel->setText(tr("No import widget available for type: %1").arg(data_type));
    }
}

QWidget * DataImport_Widget::_getOrCreateWidgetForType(QString const & data_type) {
    // Check cache first
    auto it = _type_widgets.find(data_type);
    if (it != _type_widgets.end()) {
        return it->second;
    }

    // Create new widget from registry
    auto const & registry = DataImportTypeRegistry::instance();
    QWidget * widget = registry.createWidget(data_type, _data_manager, this);

    if (widget) {
        // Add to stacked widget and cache
        ui->stackedWidget->addWidget(widget);
        _type_widgets[data_type] = widget;
    }

    return widget;
}

void DataImport_Widget::_updateHeader(QString const & data_type) {
    if (data_type.isEmpty()) {
        ui->headerLabel->setText(tr("Data Import"));
        setWindowTitle(tr("Data Import"));
    } else {
        auto const & registry = DataImportTypeRegistry::instance();
        QString const display_name = registry.displayName(data_type);
        QString const title = display_name.isEmpty() ? data_type : display_name;

        ui->headerLabel->setText(tr("Import %1").arg(title));
        setWindowTitle(tr("Import %1").arg(title));
    }
}
