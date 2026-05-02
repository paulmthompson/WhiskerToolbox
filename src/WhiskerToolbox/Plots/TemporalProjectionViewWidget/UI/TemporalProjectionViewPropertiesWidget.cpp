#include "TemporalProjectionViewPropertiesWidget.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "Core/TemporalProjectionViewState.hpp"
#include "DataManager/DataManager.hpp"
#include "Lines/Line_Data.hpp"
#include "Plots/Common/GlyphStyleWidget/GlyphStyleControls.hpp"
#include "Plots/Common/LineStyleControls/LineStyleControls.hpp"
#include "Plots/Common/HorizontalAxisWidget/HorizontalAxisWithRangeControls.hpp"
#include "Plots/Common/SelectionInstructions.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "Points/Point_Data.hpp"
#include "UI/TemporalProjectionViewWidget.hpp"

#include "ui_TemporalProjectionViewPropertiesWidget.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>

#include <algorithm>

TemporalProjectionViewPropertiesWidget::TemporalProjectionViewPropertiesWidget(
    std::shared_ptr<TemporalProjectionViewState> state,
    std::shared_ptr<DataManager> data_manager,
    QWidget * parent)
    : QWidget(parent),
      ui(new Ui::TemporalProjectionViewPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _plot_widget(nullptr),
      _horizontal_range_controls(nullptr),
      _horizontal_range_controls_section(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_range_controls_section(nullptr),
      _glyph_style_controls(nullptr),
      _line_style_controls(nullptr),
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

    // Connect UI signals — point data
    connect(ui->add_point_button, &QPushButton::clicked,
            this, &TemporalProjectionViewPropertiesWidget::_onAddPointClicked);
    connect(ui->remove_point_button, &QPushButton::clicked,
            this, &TemporalProjectionViewPropertiesWidget::_onRemovePointClicked);
    connect(ui->point_data_table, &QTableWidget::itemSelectionChanged,
            this, &TemporalProjectionViewPropertiesWidget::_onPointTableSelectionChanged);

    // Connect UI signals — line data
    connect(ui->add_line_button, &QPushButton::clicked,
            this, &TemporalProjectionViewPropertiesWidget::_onAddLineClicked);
    connect(ui->remove_line_button, &QPushButton::clicked,
            this, &TemporalProjectionViewPropertiesWidget::_onRemoveLineClicked);
    connect(ui->line_data_table, &QTableWidget::itemSelectionChanged,
            this, &TemporalProjectionViewPropertiesWidget::_onLineTableSelectionChanged);

    // Connect UI signals — rendering
    // Point size spinbox is superseded by GlyphStyleControls;
    // hide the old spinbox and add controls programmatically
    ui->point_size_spinbox->setVisible(false);
    if (auto * label = ui->point_size_label) {
        label->setVisible(false);
    }
    // Line width spinbox is superseded by LineStyleControls;
    // hide the old spinbox and add controls programmatically
    ui->line_width_spinbox->setVisible(false);
    if (auto * label = ui->line_width_label) {
        label->setVisible(false);
    }

    // Connect UI signals — selection mode
    connect(ui->selection_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TemporalProjectionViewPropertiesWidget::_onSelectionModeChanged);
    connect(ui->clear_selection_button, &QPushButton::clicked,
            this, &TemporalProjectionViewPropertiesWidget::_onClearSelectionClicked);

    // Connect UI signals — color by group
    connect(ui->color_by_group_checkbox, &QCheckBox::toggled,
            this, &TemporalProjectionViewPropertiesWidget::_onColorByGroupToggled);

    // Populate combo boxes
    _populatePointComboBox();
    _populateLineComboBox();

    // Set up DataManager observer to refresh combo boxes when data changes
    if (_data_manager) {
        _dm_observer_id = _data_manager->addObserver([this]() {
            _populatePointComboBox();
            _populateLineComboBox();
        }, "TemporalProjectionViewPropertiesWidget");
    }

    // Connect state signals
    if (_state) {
        connect(_state.get(), &TemporalProjectionViewState::pointDataKeyAdded,
                this, &TemporalProjectionViewPropertiesWidget::_onStatePointKeyAdded);
        connect(_state.get(), &TemporalProjectionViewState::pointDataKeyRemoved,
                this, &TemporalProjectionViewPropertiesWidget::_onStatePointKeyRemoved);
        connect(_state.get(), &TemporalProjectionViewState::pointDataKeysCleared,
                this, [this]() { _updatePointDataTable(); });
        connect(_state.get(), &TemporalProjectionViewState::lineDataKeyAdded,
                this, &TemporalProjectionViewPropertiesWidget::_onStateLineKeyAdded);
        connect(_state.get(), &TemporalProjectionViewState::lineDataKeyRemoved,
                this, &TemporalProjectionViewPropertiesWidget::_onStateLineKeyRemoved);
        connect(_state.get(), &TemporalProjectionViewState::lineDataKeysCleared,
                this, [this]() { _updateLineDataTable(); });
        connect(_state.get(), &TemporalProjectionViewState::glyphStyleChanged,
                this, [this]() {
                    // GlyphStyleControls self-updates via GlyphStyleState signals
                });
        connect(_state.get(), &TemporalProjectionViewState::lineStyleChanged,
                this, [this]() {
                    // LineStyleControls self-updates via LineStyleState signals
                });
        connect(_state.get(), &TemporalProjectionViewState::selectionModeChanged,
                this, [this](QString const & mode) {
                    ui->selection_mode_combo->blockSignals(true);
                    if (mode == "point") {
                        ui->selection_mode_combo->setCurrentIndex(1);
                    } else if (mode == "line") {
                        ui->selection_mode_combo->setCurrentIndex(2);
                    } else if (mode == "polygon") {
                        ui->selection_mode_combo->setCurrentIndex(3);
                    } else {
                        ui->selection_mode_combo->setCurrentIndex(0);
                    }
                    ui->selection_mode_combo->blockSignals(false);
                    _updateSelectionInstructions();
                });
        connect(_state.get(), &TemporalProjectionViewState::colorByGroupChanged,
                this, [this]() {
                    ui->color_by_group_checkbox->blockSignals(true);
                    ui->color_by_group_checkbox->setChecked(_state->colorByGroup());
                    ui->color_by_group_checkbox->blockSignals(false);
                });

        _updateUIFromState();

        // Create GlyphStyleControls and insert into layout
        if (!_glyph_style_controls) {
            _glyph_style_controls = new GlyphStyleControls(
                _state->glyphStyleState(), this);
            // Insert before the line width controls in the rendering section
            auto * rendering_layout = ui->point_size_spinbox->parentWidget()->layout();
            if (rendering_layout) {
                rendering_layout->addWidget(_glyph_style_controls);
            }
        }

        // Create LineStyleControls and insert into layout
        if (!_line_style_controls) {
            _line_style_controls = new LineStyleControls(
                _state->lineStyleState(), this);
            auto * rendering_layout = ui->line_width_spinbox->parentWidget()->layout();
            if (rendering_layout) {
                rendering_layout->addWidget(_line_style_controls);
            }
        }
    }
}

TemporalProjectionViewPropertiesWidget::~TemporalProjectionViewPropertiesWidget()
{
    if (_data_manager && _dm_observer_id != -1) {
        _data_manager->removeObserver(_dm_observer_id);
    }
    delete ui;
}

void TemporalProjectionViewPropertiesWidget::setPlotWidget(TemporalProjectionViewWidget * plot_widget)
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

void TemporalProjectionViewPropertiesWidget::_populatePointComboBox()
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

void TemporalProjectionViewPropertiesWidget::_onAddPointClicked()
{
    if (!_state) {
        return;
    }
    QString const key = ui->add_point_combo->currentText();
    if (!key.isEmpty()) {
        _state->addPointDataKey(key);
    }
}

void TemporalProjectionViewPropertiesWidget::_onRemovePointClicked()
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

void TemporalProjectionViewPropertiesWidget::_onPointTableSelectionChanged()
{
    bool has_selection = !ui->point_data_table->selectedItems().isEmpty();
    ui->remove_point_button->setEnabled(has_selection);
}

void TemporalProjectionViewPropertiesWidget::_onStatePointKeyAdded(QString const & /*key*/)
{
    _updatePointDataTable();
}

void TemporalProjectionViewPropertiesWidget::_onStatePointKeyRemoved(QString const & /*key*/)
{
    _updatePointDataTable();
    ui->point_data_table->clearSelection();
    ui->remove_point_button->setEnabled(false);
}

void TemporalProjectionViewPropertiesWidget::_updatePointDataTable()
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

    // Resize table height to fit content
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

void TemporalProjectionViewPropertiesWidget::_populateLineComboBox()
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

void TemporalProjectionViewPropertiesWidget::_onAddLineClicked()
{
    if (!_state) {
        return;
    }
    QString const key = ui->add_line_combo->currentText();
    if (!key.isEmpty()) {
        _state->addLineDataKey(key);
    }
}

void TemporalProjectionViewPropertiesWidget::_onRemoveLineClicked()
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

void TemporalProjectionViewPropertiesWidget::_onLineTableSelectionChanged()
{
    bool has_selection = !ui->line_data_table->selectedItems().isEmpty();
    ui->remove_line_button->setEnabled(has_selection);
}

void TemporalProjectionViewPropertiesWidget::_onStateLineKeyAdded(QString const & /*key*/)
{
    _updateLineDataTable();
}

void TemporalProjectionViewPropertiesWidget::_onStateLineKeyRemoved(QString const & /*key*/)
{
    _updateLineDataTable();
    ui->line_data_table->clearSelection();
    ui->remove_line_button->setEnabled(false);
}

void TemporalProjectionViewPropertiesWidget::_updateLineDataTable()
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

    // Resize table height to fit content
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
// Selection Mode
// =============================================================================

void TemporalProjectionViewPropertiesWidget::_onSelectionModeChanged(int index)
{
    if (!_state) {
        return;
    }
    switch (index) {
    case 0:
        _state->setSelectionMode(QStringLiteral("none"));
        break;
    case 1:
        _state->setSelectionMode(QStringLiteral("point"));
        break;
    case 2:
        _state->setSelectionMode(QStringLiteral("line"));
        break;
    case 3:
        _state->setSelectionMode(QStringLiteral("polygon"));
        break;
    default:
        _state->setSelectionMode(QStringLiteral("none"));
        break;
    }
    _updateSelectionInstructions();
}

void TemporalProjectionViewPropertiesWidget::_onClearSelectionClicked()
{
    if (_plot_widget) {
        _plot_widget->clearSelection();
    }
}

void TemporalProjectionViewPropertiesWidget::_updateSelectionInstructions()
{
    if (!ui->selection_instructions_label) {
        return;
    }

    int const idx = ui->selection_mode_combo->currentIndex();
    switch (idx) {
    case 1:  // Point
        ui->selection_instructions_label->setText(
            WhiskerToolbox::Plots::SelectionInstructions::singlePoint());
        break;
    case 2:  // Line
        ui->selection_instructions_label->setText(
            WhiskerToolbox::Plots::SelectionInstructions::lineCrossing());
        break;
    case 3:  // Polygon
        ui->selection_instructions_label->setText(
            WhiskerToolbox::Plots::SelectionInstructions::polygon());
        break;
    default:  // None
        ui->selection_instructions_label->setText(
            WhiskerToolbox::Plots::SelectionInstructions::none());
        break;
    }
}

void TemporalProjectionViewPropertiesWidget::_onColorByGroupToggled(bool checked)
{
    if (_state) {
        _state->setColorByGroup(checked);
    }
}

// =============================================================================
// Internal Helpers
// =============================================================================

void TemporalProjectionViewPropertiesWidget::_updateUIFromState()
{
    if (!_state) {
        return;
    }

    // Point style is managed by GlyphStyleControls (self-syncs via GlyphStyleState)
    // Line style is managed by LineStyleControls (self-syncs via LineStyleState)

    // Selection mode
    ui->selection_mode_combo->blockSignals(true);
    QString mode = _state->getSelectionMode();
    if (mode == "point") {
        ui->selection_mode_combo->setCurrentIndex(1);
    } else if (mode == "line") {
        ui->selection_mode_combo->setCurrentIndex(2);
    } else if (mode == "polygon") {
        ui->selection_mode_combo->setCurrentIndex(3);
    } else {
        ui->selection_mode_combo->setCurrentIndex(0);
    }
    ui->selection_mode_combo->blockSignals(false);
    _updateSelectionInstructions();

    // Color by group
    ui->color_by_group_checkbox->blockSignals(true);
    ui->color_by_group_checkbox->setChecked(_state->colorByGroup());
    ui->color_by_group_checkbox->blockSignals(false);

    // Data key tables
    _updatePointDataTable();
    _updateLineDataTable();
}
