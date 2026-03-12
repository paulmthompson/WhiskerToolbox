#include "LabelConfigPanel.hpp"

#include "MLCoreWidgetState.hpp"
#include "ui_LabelConfigPanel.h"

#include "DataManager/DataManager.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Entity/EntityGroupManager.hpp"

#include <QTimer>

#include <algorithm>

// =============================================================================
// Construction / destruction
// =============================================================================

LabelConfigPanel::LabelConfigPanel(
        std::shared_ptr<MLCoreWidgetState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent)
    : QWidget(parent),
      ui(new Ui::LabelConfigPanel),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)) {
    ui->setupUi(this);
    _setupConnections();
    _registerObservers();

    // Populate combo boxes
    _refreshIntervalCombo();
    _refreshGroupCombo();
    _refreshDataKeyCombo();
    _refreshDataGroupCombo();

    // Restore from saved state
    _restoreFromState();

    // Initial validation
    _updateValidation();
}

LabelConfigPanel::~LabelConfigPanel() {
    if (_dm_observer_id >= 0 && _data_manager) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    if (_group_observer_id >= 0 && _data_manager) {
        auto * group_mgr = _data_manager->getEntityGroupManager();
        group_mgr->getGroupObservers().removeObserver(_group_observer_id);
    }
    delete ui;
}

// =============================================================================
// Public API
// =============================================================================

bool LabelConfigPanel::hasValidSelection() const {
    return _valid;
}

std::string LabelConfigPanel::labelSourceType() const {
    if (ui->intervalRadio->isChecked()) {
        return "intervals";
    }
    if (ui->groupsRadio->isChecked()) {
        return "groups";
    }
    return "entity_groups";
}

std::vector<uint64_t> LabelConfigPanel::selectedGroupIds() const {
    if (ui->groupsRadio->isChecked()) {
        return _selected_group_ids;
    }
    if (ui->dataGroupsRadio->isChecked()) {
        return _selected_data_group_ids;
    }
    return {};
}

void LabelConfigPanel::refreshAll() {
    _refreshIntervalCombo();
    _refreshGroupCombo();
    _refreshDataKeyCombo();
    _refreshDataGroupCombo();
    _rebuildClassList();
    _rebuildDataClassList();
    _updateValidation();
}

// =============================================================================
// Radio button slots
// =============================================================================

void LabelConfigPanel::_onIntervalRadioToggled(bool checked) {
    if (!checked) {
        return;
    }
    ui->contentStack->setCurrentIndex(0);
    _syncSourceTypeToState();
    _updateValidation();
    emit labelSourceChanged(QStringLiteral("intervals"));
}

void LabelConfigPanel::_onGroupsRadioToggled(bool checked) {
    if (!checked) {
        return;
    }
    ui->contentStack->setCurrentIndex(1);
    _syncSourceTypeToState();
    _updateValidation();
    emit labelSourceChanged(QStringLiteral("groups"));
}

void LabelConfigPanel::_onDataGroupsRadioToggled(bool checked) {
    if (!checked) {
        return;
    }
    ui->contentStack->setCurrentIndex(2);
    _syncSourceTypeToState();
    _updateValidation();
    emit labelSourceChanged(QStringLiteral("entity_groups"));
}

// =============================================================================
// Interval mode slots
// =============================================================================

void LabelConfigPanel::_onIntervalComboChanged(int index) {
    QString key_str;
    if (index >= 0) {
        QVariant data = ui->intervalComboBox->itemData(index);
        if (data.isValid()) {
            key_str = data.toString();
        } else {
            key_str = ui->intervalComboBox->itemText(index);
        }
    }

    if (_state) {
        _state->setLabelIntervalKey(key_str.toStdString());
    }

    // Update interval info
    _updateIntervalInfo(key_str.toStdString());
    _updateValidation();
}

void LabelConfigPanel::_onPositiveClassNameEdited(QString const & text) {
    if (_state) {
        _state->setLabelPositiveClassName(text.toStdString());
    }
}

void LabelConfigPanel::_onNegativeClassNameEdited(QString const & text) {
    if (_state) {
        _state->setLabelNegativeClassName(text.toStdString());
    }
}

// =============================================================================
// Entity Groups mode slots
// =============================================================================

void LabelConfigPanel::_onAddGroupClicked() {
    int idx = ui->groupComboBox->currentIndex();
    if (idx < 0) {
        return;
    }

    QVariant data = ui->groupComboBox->itemData(idx);
    if (!data.isValid()) {
        return;
    }

    auto group_id = data.toULongLong();

    // Don't add duplicates
    if (std::ranges::find(_selected_group_ids, group_id) != _selected_group_ids.end()) {
        return;
    }

    _selected_group_ids.push_back(group_id);
    _syncGroupIdsToState();
    _rebuildClassList();
    _updateValidation();
}

void LabelConfigPanel::_onRemoveGroupClicked() {
    int row = ui->classListWidget->currentRow();
    if (row < 0 || row >= static_cast<int>(_selected_group_ids.size())) {
        return;
    }

    _selected_group_ids.erase(_selected_group_ids.begin() + row);
    _syncGroupIdsToState();
    _rebuildClassList();
    _updateValidation();
}

// =============================================================================
// Data Entity Groups mode slots
// =============================================================================

void LabelConfigPanel::_onDataKeyComboChanged(int index) {
    QString key_str;
    if (index >= 0) {
        QVariant data = ui->dataKeyComboBox->itemData(index);
        if (data.isValid()) {
            key_str = data.toString();
        } else {
            key_str = ui->dataKeyComboBox->itemText(index);
        }
    }

    if (_state) {
        _state->setLabelDataKey(key_str.toStdString());
    }
    _updateValidation();
}

void LabelConfigPanel::_onAddDataGroupClicked() {
    int idx = ui->dataGroupComboBox->currentIndex();
    if (idx < 0) {
        return;
    }

    QVariant data = ui->dataGroupComboBox->itemData(idx);
    if (!data.isValid()) {
        return;
    }

    auto group_id = data.toULongLong();

    // Don't add duplicates
    if (std::ranges::find(_selected_data_group_ids, group_id) !=
        _selected_data_group_ids.end()) {
        return;
    }

    _selected_data_group_ids.push_back(group_id);
    _syncGroupIdsToState();
    _rebuildDataClassList();
    _updateValidation();
}

void LabelConfigPanel::_onRemoveDataGroupClicked() {
    int row = ui->dataClassListWidget->currentRow();
    if (row < 0 || row >= static_cast<int>(_selected_data_group_ids.size())) {
        return;
    }

    _selected_data_group_ids.erase(_selected_data_group_ids.begin() + row);
    _syncGroupIdsToState();
    _rebuildDataClassList();
    _updateValidation();
}

// =============================================================================
// Connection setup
// =============================================================================

void LabelConfigPanel::_setupConnections() {
    // Radio buttons
    connect(ui->intervalRadio, &QRadioButton::toggled,
            this, &LabelConfigPanel::_onIntervalRadioToggled);
    connect(ui->groupsRadio, &QRadioButton::toggled,
            this, &LabelConfigPanel::_onGroupsRadioToggled);
    connect(ui->dataGroupsRadio, &QRadioButton::toggled,
            this, &LabelConfigPanel::_onDataGroupsRadioToggled);

    // Interval mode
    connect(ui->intervalComboBox, &QComboBox::currentIndexChanged,
            this, &LabelConfigPanel::_onIntervalComboChanged);
    connect(ui->intervalRefreshButton, &QPushButton::clicked,
            this, [this]() {
                _refreshIntervalCombo();
                _updateValidation();
            });
    connect(ui->positiveClassEdit, &QLineEdit::textEdited,
            this, &LabelConfigPanel::_onPositiveClassNameEdited);
    connect(ui->negativeClassEdit, &QLineEdit::textEdited,
            this, &LabelConfigPanel::_onNegativeClassNameEdited);

    // Entity Groups mode
    connect(ui->addGroupButton, &QPushButton::clicked,
            this, &LabelConfigPanel::_onAddGroupClicked);
    connect(ui->removeGroupButton, &QPushButton::clicked,
            this, &LabelConfigPanel::_onRemoveGroupClicked);

    // Data Entity Groups mode
    connect(ui->dataKeyComboBox, &QComboBox::currentIndexChanged,
            this, &LabelConfigPanel::_onDataKeyComboChanged);
    connect(ui->dataKeyRefreshButton, &QPushButton::clicked,
            this, [this]() {
                _refreshDataKeyCombo();
                _updateValidation();
            });
    connect(ui->addDataGroupButton, &QPushButton::clicked,
            this, &LabelConfigPanel::_onAddDataGroupClicked);
    connect(ui->removeDataGroupButton, &QPushButton::clicked,
            this, &LabelConfigPanel::_onRemoveDataGroupClicked);

    // State-driven connections (for external state changes / JSON restore)
    _setupStateConnections();
}

void LabelConfigPanel::_setupStateConnections() {
    if (!_state) {
        return;
    }

    connect(_state.get(), &MLCoreWidgetState::labelSourceTypeChanged,
            this, [this](QString const & type) {
                int page = _sourceTypeToPageIndex(type.toStdString());
                ui->contentStack->setCurrentIndex(page);

                ui->intervalRadio->blockSignals(true);
                ui->groupsRadio->blockSignals(true);
                ui->dataGroupsRadio->blockSignals(true);

                ui->intervalRadio->setChecked(page == 0);
                ui->groupsRadio->setChecked(page == 1);
                ui->dataGroupsRadio->setChecked(page == 2);

                ui->intervalRadio->blockSignals(false);
                ui->groupsRadio->blockSignals(false);
                ui->dataGroupsRadio->blockSignals(false);

                _updateValidation();
            });

    connect(_state.get(), &MLCoreWidgetState::labelIntervalKeyChanged,
            this, [this](QString const & key) {
                for (int i = 0; i < ui->intervalComboBox->count(); ++i) {
                    QVariant data = ui->intervalComboBox->itemData(i);
                    if (data.isValid() && data.toString() == key) {
                        ui->intervalComboBox->blockSignals(true);
                        ui->intervalComboBox->setCurrentIndex(i);
                        ui->intervalComboBox->blockSignals(false);
                        _updateIntervalInfo(key.toStdString());
                        _updateValidation();
                        return;
                    }
                }
                _refreshIntervalCombo();
            });

    connect(_state.get(), &MLCoreWidgetState::labelPositiveClassNameChanged,
            this, [this](QString const & name) {
                if (ui->positiveClassEdit->text() != name) {
                    ui->positiveClassEdit->blockSignals(true);
                    ui->positiveClassEdit->setText(name);
                    ui->positiveClassEdit->blockSignals(false);
                }
            });

    connect(_state.get(), &MLCoreWidgetState::labelNegativeClassNameChanged,
            this, [this](QString const & name) {
                if (ui->negativeClassEdit->text() != name) {
                    ui->negativeClassEdit->blockSignals(true);
                    ui->negativeClassEdit->setText(name);
                    ui->negativeClassEdit->blockSignals(false);
                }
            });

    connect(_state.get(), &MLCoreWidgetState::labelGroupIdsChanged,
            this, [this]() {
                if (!_state) {
                    return;
                }
                auto const & ids = _state->labelGroupIds();
                // Determine which mode's list to update based on current source type
                std::string const & source = _state->labelSourceType();
                if (source == "groups") {
                    _selected_group_ids = ids;
                    _rebuildClassList();
                } else if (source == "entity_groups") {
                    _selected_data_group_ids = ids;
                    _rebuildDataClassList();
                }
                _updateValidation();
            });

    connect(_state.get(), &MLCoreWidgetState::labelDataKeyChanged,
            this, [this](QString const & key) {
                for (int i = 0; i < ui->dataKeyComboBox->count(); ++i) {
                    QVariant data = ui->dataKeyComboBox->itemData(i);
                    if (data.isValid() && data.toString() == key) {
                        ui->dataKeyComboBox->blockSignals(true);
                        ui->dataKeyComboBox->setCurrentIndex(i);
                        ui->dataKeyComboBox->blockSignals(false);
                        _updateValidation();
                        return;
                    }
                }
                _refreshDataKeyCombo();
            });
}

// =============================================================================
// Observer registration
// =============================================================================

void LabelConfigPanel::_registerObservers() {
    if (!_data_manager) {
        return;
    }

    // DataManager observer — refreshes interval and data key combos
    _dm_observer_id = _data_manager->addObserver([this]() {
        QTimer::singleShot(0, this, &LabelConfigPanel::refreshAll);
    });

    // EntityGroupManager observer — refreshes group combos
    auto * group_mgr = _data_manager->getEntityGroupManager();
    _group_observer_id = group_mgr->getGroupObservers().addObserver([this]() {
        QTimer::singleShot(0, this, [this]() {
            _refreshGroupCombo();
            _refreshDataGroupCombo();
            _rebuildClassList();
            _rebuildDataClassList();
            _updateValidation();
        });
    });
}

// =============================================================================
// Combo box population
// =============================================================================

void LabelConfigPanel::_refreshIntervalCombo() {
    if (!_data_manager) {
        return;
    }

    QString current_key;
    int cur_idx = ui->intervalComboBox->currentIndex();
    if (cur_idx >= 0) {
        QVariant data = ui->intervalComboBox->itemData(cur_idx);
        current_key = data.isValid() ? data.toString() : ui->intervalComboBox->itemText(cur_idx);
    }

    ui->intervalComboBox->blockSignals(true);
    ui->intervalComboBox->clear();
    ui->intervalComboBox->addItem(QString{});// empty sentinel

    auto keys = _data_manager->getKeys<DigitalIntervalSeries>();
    std::sort(keys.begin(), keys.end());

    for (auto const & key: keys) {
        auto series = _data_manager->getData<DigitalIntervalSeries>(key);
        QString display = series
                                  ? QStringLiteral("%1 (%2 intervals)")
                                            .arg(QString::fromStdString(key))
                                            .arg(series->size())
                                  : QString::fromStdString(key);
        ui->intervalComboBox->addItem(display, QString::fromStdString(key));
    }

    // Restore selection
    int restore_idx = 0;
    if (!current_key.isEmpty()) {
        for (int i = 0; i < ui->intervalComboBox->count(); ++i) {
            QVariant data = ui->intervalComboBox->itemData(i);
            if (data.isValid() && data.toString() == current_key) {
                restore_idx = i;
                break;
            }
        }
    }
    ui->intervalComboBox->setCurrentIndex(restore_idx);
    ui->intervalComboBox->blockSignals(false);

    _updateIntervalInfo(
            restore_idx > 0
                    ? ui->intervalComboBox->itemData(restore_idx).toString().toStdString()
                    : std::string{});
}

void LabelConfigPanel::_refreshGroupCombo() {
    if (!_data_manager) {
        return;
    }

    ui->groupComboBox->blockSignals(true);
    ui->groupComboBox->clear();
    ui->groupComboBox->addItem(QString{});// empty sentinel

    auto * group_mgr = _data_manager->getEntityGroupManager();
    auto descriptors = group_mgr->getAllGroupDescriptors();

    // Sort by name for consistent display
    std::sort(descriptors.begin(), descriptors.end(),
              [](auto const & a, auto const & b) { return a.name < b.name; });

    for (auto const & desc: descriptors) {
        QString display = QStringLiteral("%1 (%2 entities)")
                                  .arg(QString::fromStdString(desc.name))
                                  .arg(desc.entity_count);
        ui->groupComboBox->addItem(display, QVariant::fromValue(desc.id));
    }

    ui->groupComboBox->blockSignals(false);
}

void LabelConfigPanel::_refreshDataKeyCombo() {
    if (!_data_manager) {
        return;
    }

    QString current_key;
    int cur_idx = ui->dataKeyComboBox->currentIndex();
    if (cur_idx >= 0) {
        QVariant data = ui->dataKeyComboBox->itemData(cur_idx);
        current_key = data.isValid() ? data.toString() : ui->dataKeyComboBox->itemText(cur_idx);
    }

    ui->dataKeyComboBox->blockSignals(true);
    ui->dataKeyComboBox->clear();
    ui->dataKeyComboBox->addItem(QString{});// empty sentinel

    auto keys = _data_manager->getAllKeys();
    std::sort(keys.begin(), keys.end());

    for (auto const & key: keys) {
        ui->dataKeyComboBox->addItem(QString::fromStdString(key),
                                     QString::fromStdString(key));
    }

    // Restore selection
    int restore_idx = 0;
    if (!current_key.isEmpty()) {
        for (int i = 0; i < ui->dataKeyComboBox->count(); ++i) {
            QVariant data = ui->dataKeyComboBox->itemData(i);
            if (data.isValid() && data.toString() == current_key) {
                restore_idx = i;
                break;
            }
        }
    }
    ui->dataKeyComboBox->setCurrentIndex(restore_idx);
    ui->dataKeyComboBox->blockSignals(false);
}

void LabelConfigPanel::_refreshDataGroupCombo() {
    if (!_data_manager) {
        return;
    }

    ui->dataGroupComboBox->blockSignals(true);
    ui->dataGroupComboBox->clear();
    ui->dataGroupComboBox->addItem(QString{});// empty sentinel

    auto * group_mgr = _data_manager->getEntityGroupManager();
    auto descriptors = group_mgr->getAllGroupDescriptors();

    std::sort(descriptors.begin(), descriptors.end(),
              [](auto const & a, auto const & b) { return a.name < b.name; });

    for (auto const & desc: descriptors) {
        QString display = QStringLiteral("%1 (%2 entities)")
                                  .arg(QString::fromStdString(desc.name))
                                  .arg(desc.entity_count);
        ui->dataGroupComboBox->addItem(display, QVariant::fromValue(desc.id));
    }

    ui->dataGroupComboBox->blockSignals(false);
}

// =============================================================================
// Class list management
// =============================================================================

void LabelConfigPanel::_rebuildClassList() {
    ui->classListWidget->clear();

    if (!_data_manager) {
        return;
    }

    auto * group_mgr = _data_manager->getEntityGroupManager();

    for (std::size_t i = 0; i < _selected_group_ids.size(); ++i) {
        auto gid = _selected_group_ids[i];
        auto desc = group_mgr->getGroupDescriptor(gid);
        QString text;
        if (desc) {
            text = QStringLiteral("Class %1: \"%2\" (%3 entities)")
                           .arg(i)
                           .arg(QString::fromStdString(desc->name))
                           .arg(desc->entity_count);
        } else {
            text = QStringLiteral("Class %1: <deleted group %2>").arg(i).arg(gid);
        }
        ui->classListWidget->addItem(text);
    }
}

void LabelConfigPanel::_rebuildDataClassList() {
    ui->dataClassListWidget->clear();

    if (!_data_manager) {
        return;
    }

    auto * group_mgr = _data_manager->getEntityGroupManager();

    for (std::size_t i = 0; i < _selected_data_group_ids.size(); ++i) {
        auto gid = _selected_data_group_ids[i];
        auto desc = group_mgr->getGroupDescriptor(gid);
        QString text;
        if (desc) {
            text = QStringLiteral("Class %1: \"%2\" (%3 entities)")
                           .arg(i)
                           .arg(QString::fromStdString(desc->name))
                           .arg(desc->entity_count);
        } else {
            text = QStringLiteral("Class %1: <deleted group %2>").arg(i).arg(gid);
        }
        ui->dataClassListWidget->addItem(text);
    }
}

// =============================================================================
// Interval info display
// =============================================================================

void LabelConfigPanel::_updateIntervalInfo(std::string const & key) {
    if (key.empty() || !_data_manager) {
        ui->intervalInfoLabel->setText(QString{});
        return;
    }

    auto series = _data_manager->getData<DigitalIntervalSeries>(key);
    if (!series) {
        ui->intervalInfoLabel->setText(
                QStringLiteral("<span style='color: red;'>Interval series \"%1\" not found</span>")
                        .arg(QString::fromStdString(key)));
        return;
    }

    std::size_t interval_count = series->size();

    int64_t total_frames = 0;
    for (auto const & iwid: series->view()) {
        int64_t span = iwid.interval.end - iwid.interval.start + 1;
        if (span > 0) {
            total_frames += span;
        }
    }

    ui->intervalInfoLabel->setText(
            QStringLiteral("<b>%1</b> intervals, <b>%2</b> frames total")
                    .arg(interval_count)
                    .arg(total_frames));
}

// =============================================================================
// Validation
// =============================================================================

void LabelConfigPanel::_updateValidation() {
    bool new_valid = false;
    QString msg;

    std::string source = labelSourceType();

    if (source == "intervals") {
        // Valid if an interval series is selected
        int idx = ui->intervalComboBox->currentIndex();
        QVariant data = (idx >= 0) ? ui->intervalComboBox->itemData(idx) : QVariant{};
        bool has_key = data.isValid() && !data.toString().isEmpty();

        if (!has_key) {
            msg = QStringLiteral(
                    "<span style='color: orange;'>Select an interval series for binary labeling</span>");
        } else {
            new_valid = true;
        }
    } else if (source == "groups") {
        // Valid if at least 2 groups are selected
        if (_selected_group_ids.size() < 2) {
            msg = QStringLiteral(
                    "<span style='color: orange;'>Select at least 2 groups for multi-class labeling</span>");
        } else {
            new_valid = true;
        }
    } else if (source == "entity_groups") {
        // Valid if data key selected and at least 2 groups
        int idx = ui->dataKeyComboBox->currentIndex();
        QVariant data = (idx >= 0) ? ui->dataKeyComboBox->itemData(idx) : QVariant{};
        bool has_data_key = data.isValid() && !data.toString().isEmpty();

        if (!has_data_key) {
            msg = QStringLiteral(
                    "<span style='color: orange;'>Select a data key for entity matching</span>");
        } else if (_selected_data_group_ids.size() < 2) {
            msg = QStringLiteral(
                    "<span style='color: orange;'>Select at least 2 groups for multi-class labeling</span>");
        } else {
            new_valid = true;
        }
    }

    ui->validationLabel->setText(msg);

    if (new_valid != _valid) {
        _valid = new_valid;
        emit validityChanged(_valid);
    }
}

// =============================================================================
// State synchronization
// =============================================================================

void LabelConfigPanel::_restoreFromState() {
    if (!_state) {
        return;
    }

    // Restore source type → radio button + stacked page
    std::string const & source = _state->labelSourceType();
    int page = _sourceTypeToPageIndex(source);

    ui->intervalRadio->blockSignals(true);
    ui->groupsRadio->blockSignals(true);
    ui->dataGroupsRadio->blockSignals(true);

    ui->intervalRadio->setChecked(page == 0);
    ui->groupsRadio->setChecked(page == 1);
    ui->dataGroupsRadio->setChecked(page == 2);

    ui->intervalRadio->blockSignals(false);
    ui->groupsRadio->blockSignals(false);
    ui->dataGroupsRadio->blockSignals(false);

    ui->contentStack->setCurrentIndex(page);

    // Restore interval selection
    if (!_state->labelIntervalKey().empty()) {
        QString saved_key = QString::fromStdString(_state->labelIntervalKey());
        for (int i = 0; i < ui->intervalComboBox->count(); ++i) {
            QVariant data = ui->intervalComboBox->itemData(i);
            if (data.isValid() && data.toString() == saved_key) {
                ui->intervalComboBox->blockSignals(true);
                ui->intervalComboBox->setCurrentIndex(i);
                ui->intervalComboBox->blockSignals(false);
                _updateIntervalInfo(saved_key.toStdString());
                break;
            }
        }
    }

    // Restore class names
    ui->positiveClassEdit->setText(
            QString::fromStdString(_state->labelPositiveClassName()));
    ui->negativeClassEdit->setText(
            QString::fromStdString(_state->labelNegativeClassName()));

    // Restore group IDs
    auto const & ids = _state->labelGroupIds();
    if (source == "groups") {
        _selected_group_ids = ids;
        _rebuildClassList();
    } else if (source == "entity_groups") {
        _selected_data_group_ids = ids;
        _rebuildDataClassList();
    }

    // Restore data key
    if (!_state->labelDataKey().empty()) {
        QString saved_key = QString::fromStdString(_state->labelDataKey());
        for (int i = 0; i < ui->dataKeyComboBox->count(); ++i) {
            QVariant data = ui->dataKeyComboBox->itemData(i);
            if (data.isValid() && data.toString() == saved_key) {
                ui->dataKeyComboBox->blockSignals(true);
                ui->dataKeyComboBox->setCurrentIndex(i);
                ui->dataKeyComboBox->blockSignals(false);
                break;
            }
        }
    }
}

void LabelConfigPanel::_syncSourceTypeToState() {
    if (!_state) {
        return;
    }
    _state->setLabelSourceType(labelSourceType());
}

void LabelConfigPanel::_syncGroupIdsToState() {
    if (!_state) {
        return;
    }

    // Sync whichever group ID list is active based on current source type
    std::string source = labelSourceType();
    if (source == "groups") {
        _state->setLabelGroupIds(_selected_group_ids);
    } else if (source == "entity_groups") {
        _state->setLabelGroupIds(_selected_data_group_ids);
    }
}

// =============================================================================
// Utilities
// =============================================================================

int LabelConfigPanel::_sourceTypeToPageIndex(std::string const & type) {
    if (type == "intervals") {
        return 0;
    }
    if (type == "entity_groups") {
        return 2;
    }
    return 1;// "groups" or default
}
