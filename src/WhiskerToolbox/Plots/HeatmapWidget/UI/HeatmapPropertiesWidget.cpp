#include "HeatmapPropertiesWidget.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "Core/HeatmapState.hpp"
#include "DataManager/DataManager.hpp"
#include "Feature_Tree_Widget/Feature_Tree_Widget.hpp"
#include "Plots/Common/ColormapControls/ColormapControls.hpp"
#include "Plots/Common/EventRateEstimation/RateEstimate.hpp"
#include "Plots/Common/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"
#include "Plots/Common/RateEstimationControls/EstimationMethodControls.hpp"
#include "Plots/Common/RateEstimationControls/ScalingModeControls.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "UI/HeatmapWidget.hpp"

#include "ui_HeatmapPropertiesWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <utility>

HeatmapPropertiesWidget::HeatmapPropertiesWidget(std::shared_ptr<HeatmapState> state,
                                                 std::shared_ptr<DataManager> data_manager,
                                                 QWidget * parent)
    : QWidget(parent),
      ui(new Ui::HeatmapPropertiesWidget),
      _state(std::move(state)),
      _data_manager(std::move(data_manager)),
      _alignment_widget(nullptr),
      _unit_tree_widget(nullptr),
      _plot_widget(nullptr),
      _range_controls(nullptr),
      _range_controls_section(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_range_controls_section(nullptr) {
    ui->setupUi(this);

    // Create and add PlotAlignmentWidget
    _alignment_widget = new PlotAlignmentWidget(_state->alignmentState(), _data_manager, this);
    int const alignment_index = ui->main_layout->indexOf(ui->alignment_widget_placeholder);
    ui->main_layout->removeWidget(ui->alignment_widget_placeholder);
    ui->alignment_widget_placeholder->deleteLater();
    ui->main_layout->insertWidget(alignment_index, _alignment_widget);

    // Set up the unit selection tree
    _setupUnitTree();

    // Set up the scaling / color range section
    _setupScalingSection();

    // Set up the sorting section
    _setupSortingSection();

    _setupExportSection();
}

HeatmapPropertiesWidget::~HeatmapPropertiesWidget() {
    delete ui;
}

void HeatmapPropertiesWidget::setPlotWidget(HeatmapWidget * plot_widget) {
    _plot_widget = plot_widget;
    if (!_plot_widget || !_state) {
        return;
    }

    auto * time_axis_state = _state->relativeTimeAxisState();
    if (time_axis_state) {
        _range_controls_section = new Section(this, "Time Axis Range Controls");
        _range_controls =
                new RelativeTimeAxisRangeControls(time_axis_state, _range_controls_section);
        _range_controls_section->autoSetContentLayout();
        int const insert_index = ui->main_layout->indexOf(_alignment_widget) + 1;
        ui->main_layout->insertWidget(insert_index, _range_controls_section);
    }

    auto * vertical_axis_state = _plot_widget->getVerticalAxisState();
    if (vertical_axis_state) {
        _vertical_range_controls_section = new Section(this, "Y-Axis Range Controls");
        _vertical_range_controls =
                new VerticalAxisRangeControls(vertical_axis_state, _vertical_range_controls_section);
        _vertical_range_controls_section->autoSetContentLayout();
        int const insert_index = _range_controls_section
                                         ? ui->main_layout->indexOf(_range_controls_section) + 1
                                         : ui->main_layout->indexOf(_alignment_widget) + 1;
        ui->main_layout->insertWidget(insert_index, _vertical_range_controls_section);
    }
}

void HeatmapPropertiesWidget::_setupUnitTree() {
    _unit_tree_widget = new Feature_Tree_Widget(this);

    // Filter to only show DigitalEventSeries keys
    _unit_tree_widget->setTypeFilters({DM_DataType::DigitalEvent});

    // No data type grouping since we only show one type
    _unit_tree_widget->setOrganizeByDataType(false);

    // Set the data manager — this triggers initial population
    _unit_tree_widget->setDataManager(_data_manager);

    // Replace the placeholder widget with the tree
    int const tree_index = ui->main_layout->indexOf(ui->unit_tree_placeholder);
    ui->main_layout->removeWidget(ui->unit_tree_placeholder);
    ui->unit_tree_placeholder->deleteLater();
    ui->main_layout->insertWidget(tree_index, _unit_tree_widget);

    _connectUnitTreeSignals();

    // Sync tree checkboxes with existing state
    _syncTreeFromState();
}

void HeatmapPropertiesWidget::_connectUnitTreeSignals() {
    if (!_unit_tree_widget || !_state) {
        return;
    }

    // Single unit add/remove
    connect(_unit_tree_widget, &Feature_Tree_Widget::addFeature,
            this, [this](std::string const & key) {
                _state->addUnit(key);
            });

    connect(_unit_tree_widget, &Feature_Tree_Widget::removeFeature,
            this, [this](std::string const & key) {
                _state->removeUnit(key);
            });

    // Batch unit add/remove (from group checkbox toggling)
    connect(_unit_tree_widget, &Feature_Tree_Widget::addFeatures,
            this, [this](std::vector<std::string> const & keys) {
                _state->addUnits(keys);
            });

    connect(_unit_tree_widget, &Feature_Tree_Widget::removeFeatures,
            this, [this](std::vector<std::string> const & keys) {
                _state->removeUnits(keys);
            });
}

void HeatmapPropertiesWidget::_syncTreeFromState() {
    if (!_unit_tree_widget || !_state) {
        return;
    }

    // The Feature_Tree_Widget uses its internal enabled state from checkboxes.
    // After construction, we need to check the boxes of any units already
    // present in HeatmapState (e.g. restored from JSON serialization).
    auto const & unit_keys = _state->unitKeys();
    if (unit_keys.empty()) {
        return;
    }

    // Access the underlying QTreeWidget and programmatically check items
    auto * tree = _unit_tree_widget->treeWidget();
    if (!tree) {
        return;
    }

    tree->blockSignals(true);

    std::function<void(QTreeWidgetItem *)> checkItems = [&](QTreeWidgetItem * item) {
        if (!item) return;

        std::string const key = item->text(0).toStdString();

        // Check if this is a leaf key that should be enabled
        if (item->childCount() == 0) {
            bool const should_enable =
                    std::find(unit_keys.begin(), unit_keys.end(), key) != unit_keys.end();
            item->setCheckState(1, should_enable ? Qt::Checked : Qt::Unchecked);
        }

        // Recurse into children
        for (int i = 0; i < item->childCount(); ++i) {
            checkItems(item->child(i));
        }

        // Update parent state after processing children
        if (item->childCount() > 0) {
            bool allChecked = true;
            bool allUnchecked = true;
            for (int i = 0; i < item->childCount(); ++i) {
                if (item->child(i)->checkState(1) == Qt::Checked) {
                    allUnchecked = false;
                } else {
                    allChecked = false;
                }
            }
            if (allChecked) {
                item->setCheckState(1, Qt::Checked);
            } else if (allUnchecked) {
                item->setCheckState(1, Qt::Unchecked);
            } else {
                item->setCheckState(1, Qt::PartiallyChecked);
            }
        }
    };

    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        checkItems(tree->topLevelItem(i));
    }

    tree->blockSignals(false);
}

// =============================================================================
// Scaling Section
// =============================================================================

void HeatmapPropertiesWidget::_setupScalingSection() {
    using WhiskerToolbox::Plots::ScalingMode;

    _scaling_section = new Section(this, "Rate Estimation & Scaling");

    // --- Estimation method controls (composable widget) ---
    _estimation_controls = new EstimationMethodControls(_scaling_section);
    _estimation_controls->setParams(_state->estimationParams());

    // --- Scaling mode controls (composable widget) ---
    _scaling_controls = new ScalingModeControls(_scaling_section);
    _scaling_controls->setScalingMode(_state->scaling());

    // --- Colormap & color range controls (composable widget) ---
    _colormap_controls = new ColormapControls(_scaling_section);
    _colormap_controls->setColormapPreset(_state->colormapPreset());
    ColorRangeConfig cr_config;
    cr_config.mode = static_cast<ColorRangeConfig::Mode>(
            static_cast<int>(_state->colorRange().mode));
    cr_config.vmin = _state->colorRange().vmin;
    cr_config.vmax = _state->colorRange().vmax;
    _colormap_controls->setColorRange(cr_config);

    // --- Layout ---
    auto * layout = new QVBoxLayout();
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addWidget(_estimation_controls);
    layout->addWidget(_scaling_controls);
    layout->addWidget(_colormap_controls);

    _scaling_section->setContentLayout(*layout);

    // Insert after the unit tree but before the spacer
    int const spacer_index = ui->main_layout->indexOf(ui->vertical_spacer);
    ui->main_layout->insertWidget(spacer_index, _scaling_section);

    // --- Connect signals ---
    connect(_estimation_controls, &EstimationMethodControls::paramsChanged,
            this, [this](WhiskerToolbox::Plots::EstimationParams const & params) {
                if (!_state) return;
                _state->setEstimationParams(params);
            });

    connect(_scaling_controls, &ScalingModeControls::scalingModeChanged,
            this, [this](ScalingMode mode) {
                if (!_state) return;
                _state->setScaling(mode);
            });

    connect(_colormap_controls, &ColormapControls::colormapChanged,
            this, [this](CorePlotting::Colormaps::ColormapPreset preset) {
                if (!_state) return;
                _state->setColormapPreset(preset);
            });

    connect(_colormap_controls, &ColormapControls::colorRangeChanged,
            this, [this](ColorRangeConfig const & config) {
                if (!_state) return;
                HeatmapColorRangeConfig hcr;
                hcr.mode = static_cast<HeatmapColorRangeConfig::Mode>(
                        static_cast<int>(config.mode));
                hcr.vmin = config.vmin;
                hcr.vmax = config.vmax;
                _state->setColorRange(hcr);
            });

    // Listen for state changes (e.g. z-score auto-switching)
    connect(_state.get(), &HeatmapState::scalingChanged,
            this, [this]() { _syncScalingFromState(); });
    connect(_state.get(), &HeatmapState::colorRangeChanged,
            this, [this]() { _syncScalingFromState(); });
    connect(_state.get(), &HeatmapState::colormapChanged,
            this, [this]() { _syncScalingFromState(); });
    connect(_state.get(), &HeatmapState::estimationParamsChanged,
            this, [this]() { _syncScalingFromState(); });
}

void HeatmapPropertiesWidget::_syncScalingFromState() {
    if (!_state) return;

    // Sync estimation controls
    _estimation_controls->setParams(_state->estimationParams());

    // Sync scaling controls
    _scaling_controls->setScalingMode(_state->scaling());

    // Sync colormap & color range controls
    _colormap_controls->setColormapPreset(_state->colormapPreset());

    auto const & cr = _state->colorRange();
    ColorRangeConfig cr_config;
    cr_config.mode = static_cast<ColorRangeConfig::Mode>(static_cast<int>(cr.mode));
    cr_config.vmin = cr.vmin;
    cr_config.vmax = cr.vmax;
    _colormap_controls->setColorRange(cr_config);
}

// =============================================================================
// Sorting Section
// =============================================================================

void HeatmapPropertiesWidget::_setupSortingSection() {
    _sorting_section = new Section(this, "Sorting");

    // --- Sort mode combo ---
    _sort_mode_combo = new QComboBox(_sorting_section);
    _sort_mode_combo->setObjectName("sort_mode_combo");
    for (auto const mode: allSortModes()) {
        _sort_mode_combo->addItem(sortModeLabel(mode), static_cast<int>(mode));
    }

    // --- Ascending checkbox ---
    _sort_ascending_check = new QCheckBox("Ascending", _sorting_section);
    _sort_ascending_check->setObjectName("sort_ascending_check");

    // --- Layout ---
    auto * layout = new QFormLayout();
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);
    layout->addRow("Sort By:", _sort_mode_combo);
    layout->addRow(_sort_ascending_check);

    _sorting_section->setContentLayout(*layout);

    // Insert after the scaling section but before the spacer
    int const spacer_index = ui->main_layout->indexOf(ui->vertical_spacer);
    ui->main_layout->insertWidget(spacer_index, _sorting_section);

    // --- Sync from state ---
    _syncSortingFromState();

    // --- Connect signals ---
    connect(_sort_mode_combo, &QComboBox::currentIndexChanged,
            this, [this](int index) {
                if (!_state || index < 0) return;
                auto mode = static_cast<HeatmapSortMode>(
                        _sort_mode_combo->itemData(index).toInt());
                _state->setSortMode(mode);
            });

    connect(_sort_ascending_check, &QCheckBox::toggled,
            this, [this](bool checked) {
                if (!_state) return;
                _state->setSortAscending(checked);
            });

    // Listen for state changes
    connect(_state.get(), &HeatmapState::sortChanged,
            this, [this]() { _syncSortingFromState(); });
}

void HeatmapPropertiesWidget::_syncSortingFromState() {
    if (!_state) return;

    QSignalBlocker const mode_blocker(_sort_mode_combo);
    QSignalBlocker const asc_blocker(_sort_ascending_check);

    int const mode_idx = _sort_mode_combo->findData(
            static_cast<int>(_state->sortMode()));
    if (mode_idx >= 0) {
        _sort_mode_combo->setCurrentIndex(mode_idx);
    }

    _sort_ascending_check->setChecked(_state->sortAscending());
}

// =============================================================================
// Export
// =============================================================================

void HeatmapPropertiesWidget::_setupExportSection() {
    _export_section = new Section(this, "Export");

    auto * export_layout = new QVBoxLayout();
    export_layout->setContentsMargins(4, 4, 4, 4);
    export_layout->setSpacing(4);

    auto * export_button = new QPushButton(tr("Export SVG..."));
    export_button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    export_layout->addWidget(export_button);

    _export_section->setContentLayout(*export_layout);

    int const spacer_index = ui->main_layout->indexOf(ui->vertical_spacer);
    if (spacer_index >= 0) {
        ui->main_layout->insertWidget(spacer_index, _export_section);
    } else {
        ui->main_layout->addWidget(_export_section);
    }

    connect(export_button, &QPushButton::clicked,
            this, &HeatmapPropertiesWidget::exportSVGRequested);
}
