#include "OnionSkinViewPropertiesWidget.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "Core/OnionSkinViewState.hpp"
#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Plots/Common/HorizontalAxisWidget/HorizontalAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Points/Point_Data.hpp"
#include "UI/OnionSkinViewWidget.hpp"

#include "ui_OnionSkinViewPropertiesWidget.h"

#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <algorithm>

OnionSkinViewPropertiesWidget::OnionSkinViewPropertiesWidget(
    std::shared_ptr<OnionSkinViewState> state,
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent),
      ui(new Ui::OnionSkinViewPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _plot_widget(nullptr),
      _horizontal_range_controls(nullptr),
      _horizontal_range_controls_section(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_range_controls_section(nullptr),
      _dm_observer_id(-1)
{
    ui->setupUi(this);

    // Set up point data table
    ui->point_data_table->setColumnCount(1);
    ui->point_data_table->setHorizontalHeaderLabels(QStringList() << "Data Key");
    ui->point_data_table->horizontalHeader()->setStretchLastSection(true);
    ui->point_data_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->point_data_table->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->point_data_table->verticalHeader()->setVisible(false);
    ui->point_data_table->setShowGrid(true);

    // Set up line data table
    ui->line_data_table->setColumnCount(1);
    ui->line_data_table->setHorizontalHeaderLabels(QStringList() << "Data Key");
    ui->line_data_table->horizontalHeader()->setStretchLastSection(true);
    ui->line_data_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->line_data_table->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->line_data_table->verticalHeader()->setVisible(false);
    ui->line_data_table->setShowGrid(true);

    // Set up mask data table
    ui->mask_data_table->setColumnCount(1);
    ui->mask_data_table->setHorizontalHeaderLabels(QStringList() << "Data Key");
    ui->mask_data_table->horizontalHeader()->setStretchLastSection(true);
    ui->mask_data_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->mask_data_table->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->mask_data_table->verticalHeader()->setVisible(false);
    ui->mask_data_table->setShowGrid(true);

    // Connect UI signals — point data
    connect(ui->add_point_button, &QPushButton::clicked,
            this, &OnionSkinViewPropertiesWidget::_onAddPointClicked);
    connect(ui->remove_point_button, &QPushButton::clicked,
            this, &OnionSkinViewPropertiesWidget::_onRemovePointClicked);
    connect(ui->point_data_table, &QTableWidget::itemSelectionChanged,
            this, &OnionSkinViewPropertiesWidget::_onPointTableSelectionChanged);

    // Connect UI signals — line data
    connect(ui->add_line_button, &QPushButton::clicked,
            this, &OnionSkinViewPropertiesWidget::_onAddLineClicked);
    connect(ui->remove_line_button, &QPushButton::clicked,
            this, &OnionSkinViewPropertiesWidget::_onRemoveLineClicked);
    connect(ui->line_data_table, &QTableWidget::itemSelectionChanged,
            this, &OnionSkinViewPropertiesWidget::_onLineTableSelectionChanged);

    // Connect UI signals — mask data
    connect(ui->add_mask_button, &QPushButton::clicked,
            this, &OnionSkinViewPropertiesWidget::_onAddMaskClicked);
    connect(ui->remove_mask_button, &QPushButton::clicked,
            this, &OnionSkinViewPropertiesWidget::_onRemoveMaskClicked);
    connect(ui->mask_data_table, &QTableWidget::itemSelectionChanged,
            this, &OnionSkinViewPropertiesWidget::_onMaskTableSelectionChanged);

    // Connect UI signals — temporal window
    connect(ui->window_behind_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &OnionSkinViewPropertiesWidget::_onWindowBehindChanged);
    connect(ui->window_ahead_spinbox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &OnionSkinViewPropertiesWidget::_onWindowAheadChanged);

    // Connect UI signals — alpha curve
    connect(ui->alpha_curve_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OnionSkinViewPropertiesWidget::_onAlphaCurveChanged);
    connect(ui->min_alpha_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OnionSkinViewPropertiesWidget::_onMinAlphaChanged);
    connect(ui->max_alpha_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OnionSkinViewPropertiesWidget::_onMaxAlphaChanged);

    // Connect UI signals — rendering
    connect(ui->point_size_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OnionSkinViewPropertiesWidget::_onPointSizeChanged);
    connect(ui->line_width_spinbox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &OnionSkinViewPropertiesWidget::_onLineWidthChanged);
    connect(ui->highlight_current_checkbox, &QCheckBox::toggled,
            this, &OnionSkinViewPropertiesWidget::_onHighlightCurrentChanged);

    // Populate combo boxes
    _populatePointComboBox();
    _populateLineComboBox();
    _populateMaskComboBox();

    // Set up DataManager observer to refresh combo boxes when data changes
    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populatePointComboBox();
            _populateLineComboBox();
            _populateMaskComboBox();
        });
    }

    // Connect state signals
    if (_state) {
        // Point data key signals
        connect(_state.get(), &OnionSkinViewState::pointDataKeyAdded,
                this, &OnionSkinViewPropertiesWidget::_onStatePointKeyAdded);
        connect(_state.get(), &OnionSkinViewState::pointDataKeyRemoved,
                this, &OnionSkinViewPropertiesWidget::_onStatePointKeyRemoved);
        connect(_state.get(), &OnionSkinViewState::pointDataKeysCleared,
                this, [this]() { _updatePointDataTable(); });

        // Line data key signals
        connect(_state.get(), &OnionSkinViewState::lineDataKeyAdded,
                this, &OnionSkinViewPropertiesWidget::_onStateLineKeyAdded);
        connect(_state.get(), &OnionSkinViewState::lineDataKeyRemoved,
                this, &OnionSkinViewPropertiesWidget::_onStateLineKeyRemoved);
        connect(_state.get(), &OnionSkinViewState::lineDataKeysCleared,
                this, [this]() { _updateLineDataTable(); });

        // Mask data key signals
        connect(_state.get(), &OnionSkinViewState::maskDataKeyAdded,
                this, &OnionSkinViewPropertiesWidget::_onStateMaskKeyAdded);
        connect(_state.get(), &OnionSkinViewState::maskDataKeyRemoved,
                this, &OnionSkinViewPropertiesWidget::_onStateMaskKeyRemoved);
        connect(_state.get(), &OnionSkinViewState::maskDataKeysCleared,
                this, [this]() { _updateMaskDataTable(); });

        // Temporal window signals (state → UI sync)
        connect(_state.get(), &OnionSkinViewState::windowBehindChanged,
                this, [this](int value) {
                    ui->window_behind_spinbox->blockSignals(true);
                    ui->window_behind_spinbox->setValue(value);
                    ui->window_behind_spinbox->blockSignals(false);
                });
        connect(_state.get(), &OnionSkinViewState::windowAheadChanged,
                this, [this](int value) {
                    ui->window_ahead_spinbox->blockSignals(true);
                    ui->window_ahead_spinbox->setValue(value);
                    ui->window_ahead_spinbox->blockSignals(false);
                });

        // Alpha curve signals (state → UI sync)
        connect(_state.get(), &OnionSkinViewState::alphaCurveChanged,
                this, [this](QString const & curve) {
                    ui->alpha_curve_combo->blockSignals(true);
                    if (curve == "exponential") {
                        ui->alpha_curve_combo->setCurrentIndex(1);
                    } else if (curve == "gaussian") {
                        ui->alpha_curve_combo->setCurrentIndex(2);
                    } else {
                        ui->alpha_curve_combo->setCurrentIndex(0);
                    }
                    ui->alpha_curve_combo->blockSignals(false);
                });
        connect(_state.get(), &OnionSkinViewState::minAlphaChanged,
                this, [this](float value) {
                    ui->min_alpha_spinbox->blockSignals(true);
                    ui->min_alpha_spinbox->setValue(static_cast<double>(value));
                    ui->min_alpha_spinbox->blockSignals(false);
                });
        connect(_state.get(), &OnionSkinViewState::maxAlphaChanged,
                this, [this](float value) {
                    ui->max_alpha_spinbox->blockSignals(true);
                    ui->max_alpha_spinbox->setValue(static_cast<double>(value));
                    ui->max_alpha_spinbox->blockSignals(false);
                });

        // Rendering signals (state → UI sync)
        connect(_state.get(), &OnionSkinViewState::pointSizeChanged,
                this, [this](float size) {
                    ui->point_size_spinbox->blockSignals(true);
                    ui->point_size_spinbox->setValue(static_cast<double>(size));
                    ui->point_size_spinbox->blockSignals(false);
                });
        connect(_state.get(), &OnionSkinViewState::lineWidthChanged,
                this, [this](float width) {
                    ui->line_width_spinbox->blockSignals(true);
                    ui->line_width_spinbox->setValue(static_cast<double>(width));
                    ui->line_width_spinbox->blockSignals(false);
                });
        connect(_state.get(), &OnionSkinViewState::highlightCurrentChanged,
                this, [this](bool highlight) {
                    ui->highlight_current_checkbox->blockSignals(true);
                    ui->highlight_current_checkbox->setChecked(highlight);
                    ui->highlight_current_checkbox->blockSignals(false);
                });

        _updateUIFromState();
    }
}

OnionSkinViewPropertiesWidget::~OnionSkinViewPropertiesWidget()
{
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void OnionSkinViewPropertiesWidget::setPlotWidget(OnionSkinViewWidget * plot_widget)
{
    _plot_widget = plot_widget;
    if (!_plot_widget || !_state) {
        return;
    }

    auto * horizontal_axis_state = _state->horizontalAxisState();
    if (horizontal_axis_state) {
        _horizontal_range_controls_section = new Section(this, "X-Axis Range Controls");
        _horizontal_range_controls =
            new HorizontalAxisRangeControls(horizontal_axis_state, _horizontal_range_controls_section);
        _horizontal_range_controls_section->autoSetContentLayout();
        ui->main_layout->insertWidget(0, _horizontal_range_controls_section);
    }

    auto * vertical_axis_state = _state->verticalAxisState();
    if (vertical_axis_state) {
        _vertical_range_controls_section = new Section(this, "Y-Axis Range Controls");
        _vertical_range_controls =
            new VerticalAxisRangeControls(vertical_axis_state, _vertical_range_controls_section);
        _vertical_range_controls_section->autoSetContentLayout();
        int insert_index = _horizontal_range_controls_section
                               ? ui->main_layout->indexOf(_horizontal_range_controls_section) + 1
                               : 0;
        ui->main_layout->insertWidget(insert_index, _vertical_range_controls_section);
    }
}

// =============================================================================
// Point Data Key Management
// =============================================================================

void OnionSkinViewPropertiesWidget::_populatePointComboBox()
{
    ui->add_point_combo->clear();
    if (!_data_manager) {
        return;
    }
    auto keys = _data_manager->getKeys<PointData>();
    std::sort(keys.begin(), keys.end());
    for (auto const & key : keys) {
        ui->add_point_combo->addItem(QString::fromStdString(key));
    }
}

void OnionSkinViewPropertiesWidget::_onAddPointClicked()
{
    if (!_state) {
        return;
    }
    QString const key = ui->add_point_combo->currentText();
    if (!key.isEmpty()) {
        _state->addPointDataKey(key);
    }
}

void OnionSkinViewPropertiesWidget::_onRemovePointClicked()
{
    if (!_state) {
        return;
    }
    QList<QTableWidgetItem *> selected = ui->point_data_table->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    int row = selected.first()->row();
    QTableWidgetItem * item = ui->point_data_table->item(row, 0);
    if (item) {
        _state->removePointDataKey(item->text());
    }
}

void OnionSkinViewPropertiesWidget::_onPointTableSelectionChanged()
{
    bool has_selection = !ui->point_data_table->selectedItems().isEmpty();
    ui->remove_point_button->setEnabled(has_selection);
}

void OnionSkinViewPropertiesWidget::_onStatePointKeyAdded(QString const & /*key*/)
{
    _updatePointDataTable();
}

void OnionSkinViewPropertiesWidget::_onStatePointKeyRemoved(QString const & /*key*/)
{
    _updatePointDataTable();
    ui->point_data_table->clearSelection();
    ui->remove_point_button->setEnabled(false);
}

void OnionSkinViewPropertiesWidget::_updatePointDataTable()
{
    if (!_state) {
        return;
    }
    ui->point_data_table->setRowCount(0);
    auto keys = _state->getPointDataKeys();
    for (auto const & key : keys) {
        int row = ui->point_data_table->rowCount();
        ui->point_data_table->insertRow(row);
        auto * item = new QTableWidgetItem(key);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        ui->point_data_table->setItem(row, 0, item);
    }
    ui->point_data_table->resizeRowsToContents();
    int row_count = ui->point_data_table->rowCount();
    int header_height = ui->point_data_table->horizontalHeader()->height();
    if (row_count == 0) {
        ui->point_data_table->setMinimumHeight(header_height);
        ui->point_data_table->setMaximumHeight(header_height);
    } else {
        int row_height = ui->point_data_table->rowHeight(0);
        int total_height = header_height + (row_height * row_count);
        ui->point_data_table->setMinimumHeight(total_height);
        ui->point_data_table->setMaximumHeight(total_height);
    }
}

// =============================================================================
// Line Data Key Management
// =============================================================================

void OnionSkinViewPropertiesWidget::_populateLineComboBox()
{
    ui->add_line_combo->clear();
    if (!_data_manager) {
        return;
    }
    auto keys = _data_manager->getKeys<LineData>();
    std::sort(keys.begin(), keys.end());
    for (auto const & key : keys) {
        ui->add_line_combo->addItem(QString::fromStdString(key));
    }
}

void OnionSkinViewPropertiesWidget::_onAddLineClicked()
{
    if (!_state) {
        return;
    }
    QString const key = ui->add_line_combo->currentText();
    if (!key.isEmpty()) {
        _state->addLineDataKey(key);
    }
}

void OnionSkinViewPropertiesWidget::_onRemoveLineClicked()
{
    if (!_state) {
        return;
    }
    QList<QTableWidgetItem *> selected = ui->line_data_table->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    int row = selected.first()->row();
    QTableWidgetItem * item = ui->line_data_table->item(row, 0);
    if (item) {
        _state->removeLineDataKey(item->text());
    }
}

void OnionSkinViewPropertiesWidget::_onLineTableSelectionChanged()
{
    bool has_selection = !ui->line_data_table->selectedItems().isEmpty();
    ui->remove_line_button->setEnabled(has_selection);
}

void OnionSkinViewPropertiesWidget::_onStateLineKeyAdded(QString const & /*key*/)
{
    _updateLineDataTable();
}

void OnionSkinViewPropertiesWidget::_onStateLineKeyRemoved(QString const & /*key*/)
{
    _updateLineDataTable();
    ui->line_data_table->clearSelection();
    ui->remove_line_button->setEnabled(false);
}

void OnionSkinViewPropertiesWidget::_updateLineDataTable()
{
    if (!_state) {
        return;
    }
    ui->line_data_table->setRowCount(0);
    auto keys = _state->getLineDataKeys();
    for (auto const & key : keys) {
        int row = ui->line_data_table->rowCount();
        ui->line_data_table->insertRow(row);
        auto * item = new QTableWidgetItem(key);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        ui->line_data_table->setItem(row, 0, item);
    }
    ui->line_data_table->resizeRowsToContents();
    int row_count = ui->line_data_table->rowCount();
    int header_height = ui->line_data_table->horizontalHeader()->height();
    if (row_count == 0) {
        ui->line_data_table->setMinimumHeight(header_height);
        ui->line_data_table->setMaximumHeight(header_height);
    } else {
        int row_height = ui->line_data_table->rowHeight(0);
        int total_height = header_height + (row_height * row_count);
        ui->line_data_table->setMinimumHeight(total_height);
        ui->line_data_table->setMaximumHeight(total_height);
    }
}

// =============================================================================
// Mask Data Key Management
// =============================================================================

void OnionSkinViewPropertiesWidget::_populateMaskComboBox()
{
    ui->add_mask_combo->clear();
    if (!_data_manager) {
        return;
    }
    auto keys = _data_manager->getKeys<MaskData>();
    std::sort(keys.begin(), keys.end());
    for (auto const & key : keys) {
        ui->add_mask_combo->addItem(QString::fromStdString(key));
    }
}

void OnionSkinViewPropertiesWidget::_onAddMaskClicked()
{
    if (!_state) {
        return;
    }
    QString const key = ui->add_mask_combo->currentText();
    if (!key.isEmpty()) {
        _state->addMaskDataKey(key);
    }
}

void OnionSkinViewPropertiesWidget::_onRemoveMaskClicked()
{
    if (!_state) {
        return;
    }
    QList<QTableWidgetItem *> selected = ui->mask_data_table->selectedItems();
    if (selected.isEmpty()) {
        return;
    }
    int row = selected.first()->row();
    QTableWidgetItem * item = ui->mask_data_table->item(row, 0);
    if (item) {
        _state->removeMaskDataKey(item->text());
    }
}

void OnionSkinViewPropertiesWidget::_onMaskTableSelectionChanged()
{
    bool has_selection = !ui->mask_data_table->selectedItems().isEmpty();
    ui->remove_mask_button->setEnabled(has_selection);
}

void OnionSkinViewPropertiesWidget::_onStateMaskKeyAdded(QString const & /*key*/)
{
    _updateMaskDataTable();
}

void OnionSkinViewPropertiesWidget::_onStateMaskKeyRemoved(QString const & /*key*/)
{
    _updateMaskDataTable();
    ui->mask_data_table->clearSelection();
    ui->remove_mask_button->setEnabled(false);
}

void OnionSkinViewPropertiesWidget::_updateMaskDataTable()
{
    if (!_state) {
        return;
    }
    ui->mask_data_table->setRowCount(0);
    auto keys = _state->getMaskDataKeys();
    for (auto const & key : keys) {
        int row = ui->mask_data_table->rowCount();
        ui->mask_data_table->insertRow(row);
        auto * item = new QTableWidgetItem(key);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        ui->mask_data_table->setItem(row, 0, item);
    }
    ui->mask_data_table->resizeRowsToContents();
    int row_count = ui->mask_data_table->rowCount();
    int header_height = ui->mask_data_table->horizontalHeader()->height();
    if (row_count == 0) {
        ui->mask_data_table->setMinimumHeight(header_height);
        ui->mask_data_table->setMaximumHeight(header_height);
    } else {
        int row_height = ui->mask_data_table->rowHeight(0);
        int total_height = header_height + (row_height * row_count);
        ui->mask_data_table->setMinimumHeight(total_height);
        ui->mask_data_table->setMaximumHeight(total_height);
    }
}

// =============================================================================
// Temporal Window Controls
// =============================================================================

void OnionSkinViewPropertiesWidget::_onWindowBehindChanged(int value)
{
    if (_state) {
        _state->setWindowBehind(value);
    }
}

void OnionSkinViewPropertiesWidget::_onWindowAheadChanged(int value)
{
    if (_state) {
        _state->setWindowAhead(value);
    }
}

// =============================================================================
// Alpha Curve Controls
// =============================================================================

void OnionSkinViewPropertiesWidget::_onAlphaCurveChanged(int index)
{
    if (!_state) {
        return;
    }
    switch (index) {
    case 0:
        _state->setAlphaCurve(QStringLiteral("linear"));
        break;
    case 1:
        _state->setAlphaCurve(QStringLiteral("exponential"));
        break;
    case 2:
        _state->setAlphaCurve(QStringLiteral("gaussian"));
        break;
    default:
        _state->setAlphaCurve(QStringLiteral("linear"));
        break;
    }
}

void OnionSkinViewPropertiesWidget::_onMinAlphaChanged(double value)
{
    if (_state) {
        _state->setMinAlpha(static_cast<float>(value));
    }
}

void OnionSkinViewPropertiesWidget::_onMaxAlphaChanged(double value)
{
    if (_state) {
        _state->setMaxAlpha(static_cast<float>(value));
    }
}

// =============================================================================
// Rendering Controls
// =============================================================================

void OnionSkinViewPropertiesWidget::_onPointSizeChanged(double value)
{
    if (_state) {
        _state->setPointSize(static_cast<float>(value));
    }
}

void OnionSkinViewPropertiesWidget::_onLineWidthChanged(double value)
{
    if (_state) {
        _state->setLineWidth(static_cast<float>(value));
    }
}

void OnionSkinViewPropertiesWidget::_onHighlightCurrentChanged(bool checked)
{
    if (_state) {
        _state->setHighlightCurrent(checked);
    }
}

// =============================================================================
// Internal Helpers
// =============================================================================

void OnionSkinViewPropertiesWidget::_updateUIFromState()
{
    if (!_state) {
        return;
    }

    // Temporal window
    ui->window_behind_spinbox->blockSignals(true);
    ui->window_behind_spinbox->setValue(_state->getWindowBehind());
    ui->window_behind_spinbox->blockSignals(false);

    ui->window_ahead_spinbox->blockSignals(true);
    ui->window_ahead_spinbox->setValue(_state->getWindowAhead());
    ui->window_ahead_spinbox->blockSignals(false);

    // Alpha curve
    ui->alpha_curve_combo->blockSignals(true);
    QString curve = _state->getAlphaCurve();
    if (curve == "exponential") {
        ui->alpha_curve_combo->setCurrentIndex(1);
    } else if (curve == "gaussian") {
        ui->alpha_curve_combo->setCurrentIndex(2);
    } else {
        ui->alpha_curve_combo->setCurrentIndex(0);
    }
    ui->alpha_curve_combo->blockSignals(false);

    ui->min_alpha_spinbox->blockSignals(true);
    ui->min_alpha_spinbox->setValue(static_cast<double>(_state->getMinAlpha()));
    ui->min_alpha_spinbox->blockSignals(false);

    ui->max_alpha_spinbox->blockSignals(true);
    ui->max_alpha_spinbox->setValue(static_cast<double>(_state->getMaxAlpha()));
    ui->max_alpha_spinbox->blockSignals(false);

    // Rendering
    ui->point_size_spinbox->blockSignals(true);
    ui->point_size_spinbox->setValue(static_cast<double>(_state->getPointSize()));
    ui->point_size_spinbox->blockSignals(false);

    ui->line_width_spinbox->blockSignals(true);
    ui->line_width_spinbox->setValue(static_cast<double>(_state->getLineWidth()));
    ui->line_width_spinbox->blockSignals(false);

    ui->highlight_current_checkbox->blockSignals(true);
    ui->highlight_current_checkbox->setChecked(_state->getHighlightCurrent());
    ui->highlight_current_checkbox->blockSignals(false);

    // Data key tables
    _updatePointDataTable();
    _updateLineDataTable();
    _updateMaskDataTable();
}
