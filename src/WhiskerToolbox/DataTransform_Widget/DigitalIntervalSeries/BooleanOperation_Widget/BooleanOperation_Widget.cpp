#include "BooleanOperation_Widget.hpp"

#include "ui_BooleanOperation_Widget.h"

#include "DataManager.hpp"
#include "DataManager/transforms/DigitalIntervalSeries/Digital_Interval_Boolean/digital_interval_boolean.hpp"
#include "DataManager/DigitalTimeSeries/Digital_Interval_Series.hpp"

#include <algorithm>

BooleanOperation_Widget::BooleanOperation_Widget(QWidget * parent)
    : DataManagerParameter_Widget(parent),
      ui(new Ui::BooleanOperation_Widget),
      _selected_other_series_key(),
      _current_input_key() {
    ui->setupUi(this);

    // Setup operation combo box with all available boolean operations
    ui->operationComboBox->addItem("AND", static_cast<int>(BooleanParams::BooleanOperation::AND));
    ui->operationComboBox->addItem("OR", static_cast<int>(BooleanParams::BooleanOperation::OR));
    ui->operationComboBox->addItem("XOR", static_cast<int>(BooleanParams::BooleanOperation::XOR));
    ui->operationComboBox->addItem("NOT", static_cast<int>(BooleanParams::BooleanOperation::NOT));
    ui->operationComboBox->addItem("AND_NOT", static_cast<int>(BooleanParams::BooleanOperation::AND_NOT));
    ui->operationComboBox->setCurrentIndex(0); // Default to AND

    // Connect signals
    connect(ui->operationComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BooleanOperation_Widget::_operationChanged);
    connect(ui->otherSeriesComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BooleanOperation_Widget::_otherSeriesChanged);
}

BooleanOperation_Widget::~BooleanOperation_Widget() {
    delete ui;
}

std::unique_ptr<TransformParametersBase> BooleanOperation_Widget::getParameters() const {
    auto params = std::make_unique<BooleanParams>();

    // Set the operation type
    int op_index = ui->operationComboBox->currentData().toInt();
    params->operation = static_cast<BooleanParams::BooleanOperation>(op_index);

    // For NOT operation, we don't need the other series
    if (params->operation == BooleanParams::BooleanOperation::NOT) {
        params->other_series = nullptr;
        return params;
    }

    // For other operations, we need to get the other series from DataManager
    auto dm = dataManager();
    if (dm && !_selected_other_series_key.empty()) {
        try {
            auto data_variant = dm->getDataVariant(_selected_other_series_key);
            if (data_variant.has_value() &&
                std::holds_alternative<std::shared_ptr<DigitalIntervalSeries>>(*data_variant)) {
                params->other_series = std::get<std::shared_ptr<DigitalIntervalSeries>>(*data_variant);
            } else {
                // If the selected key doesn't contain DigitalIntervalSeries, return nullptr to indicate failure
                return nullptr;
            }
        } catch (...) {
            // If any exception occurs, return nullptr to indicate failure
            return nullptr;
        }
    } else {
        // No other series key selected (and it's required for this operation), return nullptr to indicate failure
        return nullptr;
    }

    return params;
}

void BooleanOperation_Widget::onDataManagerChanged() {
    _refreshOtherSeriesKeys();
}

void BooleanOperation_Widget::onDataManagerDataChanged() {
    _refreshOtherSeriesKeys();
}

void BooleanOperation_Widget::_refreshOtherSeriesKeys() {
    auto dm = dataManager();
    if (!dm) {
        return;
    }

    // Get current DigitalIntervalSeries keys
    auto interval_keys = dm->getKeys<DigitalIntervalSeries>();

    // Update the combo box
    _updateOtherSeriesComboBox();

    // Check if we need the other series combo box for the current operation
    int op_index = ui->operationComboBox->currentData().toInt();
    auto current_op = static_cast<BooleanParams::BooleanOperation>(op_index);
    bool needs_other_series = (current_op != BooleanParams::BooleanOperation::NOT);

    // Filter out the current input key from available options
    std::vector<std::string> available_keys;
    for (auto const & key : interval_keys) {
        if (key != _current_input_key) {
            available_keys.push_back(key);
        }
    }

    // Enable/disable the combo box based on available keys and operation type
    bool has_keys = !available_keys.empty();
    ui->otherSeriesComboBox->setEnabled(has_keys && needs_other_series);

    if (has_keys && needs_other_series) {
        // Select the first available key by default if none is currently selected
        if (_selected_other_series_key.empty() ||
            std::find(available_keys.begin(), available_keys.end(), _selected_other_series_key) == available_keys.end()) {
            _selected_other_series_key = available_keys[0];
            // Find the index of the selected key in the combo box
            int index = ui->otherSeriesComboBox->findText(QString::fromStdString(_selected_other_series_key));
            if (index >= 0) {
                ui->otherSeriesComboBox->setCurrentIndex(index);
            }
        }
    } else if (!needs_other_series) {
        // NOT operation doesn't need other series
        _selected_other_series_key.clear();
    } else {
        // No keys available, clear selection
        _selected_other_series_key.clear();
        ui->otherSeriesComboBox->clear();
    }
}

void BooleanOperation_Widget::_updateOtherSeriesComboBox() {
    auto dm = dataManager();
    if (!dm) {
        return;
    }

    // Store current selection
    QString current_text = ui->otherSeriesComboBox->currentText();

    // Clear and repopulate the combo box
    ui->otherSeriesComboBox->clear();

    auto interval_keys = dm->getKeys<DigitalIntervalSeries>();
    
    // Filter out the current input key
    for (auto const & key : interval_keys) {
        if (key != _current_input_key) {
            ui->otherSeriesComboBox->addItem(QString::fromStdString(key));
        }
    }

    // Restore selection if it still exists
    if (!current_text.isEmpty()) {
        int index = ui->otherSeriesComboBox->findText(current_text);
        if (index >= 0) {
            ui->otherSeriesComboBox->setCurrentIndex(index);
        }
    }
}

void BooleanOperation_Widget::_operationChanged(int index) {
    if (index < 0) {
        return;
    }

    // Update whether we need the other series combo box
    int op_index = ui->operationComboBox->itemData(index).toInt();
    auto operation = static_cast<BooleanParams::BooleanOperation>(op_index);
    
    bool needs_other_series = (operation != BooleanParams::BooleanOperation::NOT);
    ui->otherSeriesComboBox->setEnabled(needs_other_series && ui->otherSeriesComboBox->count() > 0);

    _updateOperationDescription();
}

void BooleanOperation_Widget::_otherSeriesChanged(int index) {
    auto dm = dataManager();
    if (index >= 0 && dm) {
        _selected_other_series_key = ui->otherSeriesComboBox->itemText(index).toStdString();
    }
}

void BooleanOperation_Widget::_updateOperationDescription() {
    // This could be used to update a status label or tooltip based on the selected operation
    // Currently the description is static in the UI, but this provides a hook for dynamic updates
}
