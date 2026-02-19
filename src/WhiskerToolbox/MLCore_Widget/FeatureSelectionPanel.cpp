#include "FeatureSelectionPanel.hpp"

#include "MLCoreWidgetState.hpp"
#include "ui_FeatureSelectionPanel.h"

#include "DataManager/DataManager.hpp"
#include "DataManager/Tensors/TensorData.hpp"

#include <QTimer>

#include <algorithm>

FeatureSelectionPanel::FeatureSelectionPanel(
    std::shared_ptr<MLCoreWidgetState> state,
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent)
    , ui(new Ui::FeatureSelectionPanel)
    , _state(std::move(state))
    , _data_manager(std::move(data_manager)) {
    ui->setupUi(this);
    _setupConnections();
    _registerDataManagerObserver();

    // Initial population
    refreshTensorList();

    // Restore selection from state
    if (_state && !_state->featureTensorKey().empty()) {
        int idx = ui->tensorComboBox->findText(
            QString::fromStdString(_state->featureTensorKey()));
        if (idx != -1) {
            ui->tensorComboBox->setCurrentIndex(idx);
        }
    }
}

FeatureSelectionPanel::~FeatureSelectionPanel() {
    if (_dm_observer_id >= 0 && _data_manager) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

bool FeatureSelectionPanel::hasValidSelection() const {
    return _valid;
}

std::string FeatureSelectionPanel::selectedTensorKey() const {
    QString text = ui->tensorComboBox->currentText();
    return text.toStdString();
}

void FeatureSelectionPanel::refreshTensorList() {
    if (!_data_manager) {
        return;
    }

    QString current_selection = ui->tensorComboBox->currentText();

    ui->tensorComboBox->blockSignals(true);
    ui->tensorComboBox->clear();

    // Add empty sentinel item
    ui->tensorComboBox->addItem(QString{});

    auto tensor_keys = _data_manager->getKeys<TensorData>();
    std::sort(tensor_keys.begin(), tensor_keys.end());

    for (auto const & key : tensor_keys) {
        // Build display text with shape info
        auto tensor = _data_manager->getData<TensorData>(key);
        if (tensor) {
            QString display = QStringLiteral("%1 (%2×%3)")
                                  .arg(QString::fromStdString(key))
                                  .arg(tensor->numRows())
                                  .arg(tensor->numColumns());
            ui->tensorComboBox->addItem(display, QString::fromStdString(key));
        } else {
            ui->tensorComboBox->addItem(QString::fromStdString(key),
                                        QString::fromStdString(key));
        }
    }

    // Restore previous selection if still available
    int idx = -1;
    for (int i = 0; i < ui->tensorComboBox->count(); ++i) {
        if (ui->tensorComboBox->itemText(i) == current_selection ||
            ui->tensorComboBox->itemData(i).toString() == current_selection) {
            idx = i;
            break;
        }
    }

    if (idx != -1) {
        ui->tensorComboBox->setCurrentIndex(idx);
    } else {
        ui->tensorComboBox->setCurrentIndex(0);
    }

    ui->tensorComboBox->blockSignals(false);

    _updateTensorInfo();
}

void FeatureSelectionPanel::_onTensorComboChanged(int index) {
    // Get the actual key from item data (falls back to text if no data set)
    QString key_str;
    if (index >= 0) {
        QVariant data = ui->tensorComboBox->itemData(index);
        if (data.isValid()) {
            key_str = data.toString();
        } else {
            key_str = ui->tensorComboBox->itemText(index);
        }
    }

    std::string key = key_str.toStdString();

    // Update state
    if (_state) {
        _state->setFeatureTensorKey(key);
    }

    _updateTensorInfo();

    emit tensorSelectionChanged(key_str);
}

void FeatureSelectionPanel::_setupConnections() {
    connect(ui->tensorComboBox, &QComboBox::currentIndexChanged,
            this, &FeatureSelectionPanel::_onTensorComboChanged);

    connect(ui->refreshButton, &QPushButton::clicked,
            this, &FeatureSelectionPanel::refreshTensorList);

    // React to external state changes (e.g., from JSON restore)
    if (_state) {
        connect(_state.get(), &MLCoreWidgetState::featureTensorKeyChanged,
                this, [this](QString const & key) {
                    // Find and select the matching item
                    for (int i = 0; i < ui->tensorComboBox->count(); ++i) {
                        QVariant data = ui->tensorComboBox->itemData(i);
                        if (data.isValid() && data.toString() == key) {
                            ui->tensorComboBox->blockSignals(true);
                            ui->tensorComboBox->setCurrentIndex(i);
                            ui->tensorComboBox->blockSignals(false);
                            _updateTensorInfo();
                            return;
                        }
                    }
                    // Key not found — may need refresh
                    refreshTensorList();
                });
    }
}

void FeatureSelectionPanel::_updateTensorInfo() {
    std::string key = selectedTensorKey();

    if (key.empty() || !_data_manager) {
        ui->tensorInfoLabel->setText(QStringLiteral("No tensor selected"));
        ui->columnListLabel->setText(QString{});
        ui->validationLabel->setText(QString{});

        bool was_valid = _valid;
        _valid = false;
        if (was_valid) {
            emit validityChanged(false);
        }
        return;
    }

    auto tensor = _data_manager->getData<TensorData>(key);
    if (!tensor) {
        ui->tensorInfoLabel->setText(
            QStringLiteral("<span style='color: red;'>Tensor \"%1\" not found</span>")
                .arg(QString::fromStdString(key)));
        ui->columnListLabel->setText(QString{});
        ui->validationLabel->setText(QString{});

        bool was_valid = _valid;
        _valid = false;
        if (was_valid) {
            emit validityChanged(false);
        }
        return;
    }

    // --- Build info text ---

    // Row type
    QString row_type_str;
    switch (tensor->rowType()) {
        case RowType::TimeFrameIndex:
            row_type_str = QStringLiteral("TimeFrameIndex");
            break;
        case RowType::Interval:
            row_type_str = QStringLiteral("Interval");
            break;
        case RowType::Ordinal:
            row_type_str = QStringLiteral("Ordinal");
            break;
    }

    // Shape info
    QString info = QStringLiteral("<b>Shape:</b> %1 rows × %2 columns<br>"
                                  "<b>Row type:</b> %3")
                       .arg(tensor->numRows())
                       .arg(tensor->numColumns())
                       .arg(row_type_str);

    // Time frame info (if temporal)
    if (tensor->rowType() != RowType::Ordinal) {
        TimeKey time_key = _data_manager->getTimeKey(key);
        if (!time_key.empty()) {
            auto time_frame = _data_manager->getTime(time_key);
            QString tf_info = QStringLiteral("<br><b>Time frame:</b> %1")
                                  .arg(QString::fromStdString(time_key.str()));
            if (time_frame) {
                tf_info += QStringLiteral(" (%1 total frames)")
                               .arg(time_frame->getTotalFrameCount());
            }
            info += tf_info;
        }
    }

    ui->tensorInfoLabel->setText(info);

    // --- Column names ---
    if (tensor->hasNamedColumns()) {
        auto const & names = tensor->columnNames();
        QStringList col_list;
        col_list.reserve(static_cast<int>(names.size()));
        for (auto const & name : names) {
            col_list.append(QString::fromStdString(name));
        }

        // Truncate display if too many columns
        constexpr int max_display_cols = 20;
        QString col_text;
        if (col_list.size() <= max_display_cols) {
            col_text = QStringLiteral("<b>Columns:</b> %1").arg(col_list.join(", "));
        } else {
            QStringList truncated = col_list.mid(0, max_display_cols);
            col_text = QStringLiteral("<b>Columns:</b> %1, ... (+%2 more)")
                           .arg(truncated.join(", "))
                           .arg(col_list.size() - max_display_cols);
        }
        ui->columnListLabel->setText(col_text);
    } else {
        ui->columnListLabel->setText(
            QStringLiteral("<b>Columns:</b> %1 unnamed columns")
                .arg(tensor->numColumns()));
    }

    // --- Validation ---
    bool new_valid = (tensor->numRows() > 0 && tensor->numColumns() > 0);
    QString validation_msg;

    if (tensor->numRows() == 0) {
        validation_msg = QStringLiteral(
            "<span style='color: orange;'>Warning: tensor has 0 rows</span>");
    } else if (tensor->numColumns() == 0) {
        validation_msg = QStringLiteral(
            "<span style='color: orange;'>Warning: tensor has 0 columns</span>");
    }

    ui->validationLabel->setText(validation_msg);

    if (new_valid != _valid) {
        _valid = new_valid;
        emit validityChanged(_valid);
    }
}

void FeatureSelectionPanel::_registerDataManagerObserver() {
    if (!_data_manager) {
        return;
    }

    _dm_observer_id = _data_manager->addObserver([this]() {
        // Use a queued invocation to avoid re-entrancy during DataManager mutations
        QTimer::singleShot(0, this, &FeatureSelectionPanel::refreshTensorList);
    });
}
