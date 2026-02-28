#include "HeatmapPropertiesWidget.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "Core/HeatmapState.hpp"
#include "DataManager/DataManager.hpp"
#include "Feature_Tree_Widget/Feature_Tree_Widget.hpp"
#include "Plots/Common/EventRateEstimation/RateEstimate.hpp"
#include "Plots/Common/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "UI/HeatmapWidget.hpp"

#include "ui_HeatmapPropertiesWidget.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

HeatmapPropertiesWidget::HeatmapPropertiesWidget(std::shared_ptr<HeatmapState> state,
                                                 std::shared_ptr<DataManager> data_manager,
                                                 QWidget * parent)
    : QWidget(parent),
      ui(new Ui::HeatmapPropertiesWidget),
      _state(state),
      _data_manager(data_manager),
      _alignment_widget(nullptr),
      _unit_tree_widget(nullptr),
      _plot_widget(nullptr),
      _range_controls(nullptr),
      _range_controls_section(nullptr),
      _vertical_range_controls(nullptr),
      _vertical_range_controls_section(nullptr)
{
    ui->setupUi(this);

    // Create and add PlotAlignmentWidget
    _alignment_widget = new PlotAlignmentWidget(_state->alignmentState(), _data_manager, this);
    int alignment_index = ui->main_layout->indexOf(ui->alignment_widget_placeholder);
    ui->main_layout->removeWidget(ui->alignment_widget_placeholder);
    ui->alignment_widget_placeholder->deleteLater();
    ui->main_layout->insertWidget(alignment_index, _alignment_widget);

    // Set up the unit selection tree
    _setupUnitTree();

    // Set up the scaling / color range section
    _setupScalingSection();
}

HeatmapPropertiesWidget::~HeatmapPropertiesWidget()
{
    delete ui;
}

void HeatmapPropertiesWidget::setPlotWidget(HeatmapWidget * plot_widget)
{
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
        int insert_index = ui->main_layout->indexOf(_alignment_widget) + 1;
        ui->main_layout->insertWidget(insert_index, _range_controls_section);
    }

    auto * vertical_axis_state = _plot_widget->getVerticalAxisState();
    if (vertical_axis_state) {
        _vertical_range_controls_section = new Section(this, "Y-Axis Range Controls");
        _vertical_range_controls =
            new VerticalAxisRangeControls(vertical_axis_state, _vertical_range_controls_section);
        _vertical_range_controls_section->autoSetContentLayout();
        int insert_index = _range_controls_section
                               ? ui->main_layout->indexOf(_range_controls_section) + 1
                               : ui->main_layout->indexOf(_alignment_widget) + 1;
        ui->main_layout->insertWidget(insert_index, _vertical_range_controls_section);
    }
}

void HeatmapPropertiesWidget::_setupUnitTree()
{
    _unit_tree_widget = new Feature_Tree_Widget(this);

    // Filter to only show DigitalEventSeries keys
    _unit_tree_widget->setTypeFilters({DM_DataType::DigitalEvent});

    // No data type grouping since we only show one type
    _unit_tree_widget->setOrganizeByDataType(false);

    // Set the data manager — this triggers initial population
    _unit_tree_widget->setDataManager(_data_manager);

    // Replace the placeholder widget with the tree
    int tree_index = ui->main_layout->indexOf(ui->unit_tree_placeholder);
    ui->main_layout->removeWidget(ui->unit_tree_placeholder);
    ui->unit_tree_placeholder->deleteLater();
    ui->main_layout->insertWidget(tree_index, _unit_tree_widget);

    _connectUnitTreeSignals();

    // Sync tree checkboxes with existing state
    _syncTreeFromState();
}

void HeatmapPropertiesWidget::_connectUnitTreeSignals()
{
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

void HeatmapPropertiesWidget::_syncTreeFromState()
{
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

void HeatmapPropertiesWidget::_setupScalingSection()
{
    using WhiskerToolbox::Plots::ScalingMode;

    _scaling_section = new Section(this, "Scaling & Color Range");

    // --- Scaling mode combo ---
    _scaling_combo = new QComboBox(_scaling_section);
    _scaling_combo->setObjectName("scaling_combo");
    for (auto mode : WhiskerToolbox::Plots::allScalingModes()) {
        _scaling_combo->addItem(
            QString::fromUtf8(WhiskerToolbox::Plots::scalingLabel(mode)),
            static_cast<int>(mode));
    }

    // --- Color range mode combo ---
    _color_range_mode_combo = new QComboBox(_scaling_section);
    _color_range_mode_combo->setObjectName("color_range_mode_combo");
    _color_range_mode_combo->addItem("Auto", static_cast<int>(HeatmapColorRangeConfig::Mode::Auto));
    _color_range_mode_combo->addItem("Manual", static_cast<int>(HeatmapColorRangeConfig::Mode::Manual));
    _color_range_mode_combo->addItem("Symmetric", static_cast<int>(HeatmapColorRangeConfig::Mode::Symmetric));

    // --- Manual vmin / vmax spinboxes ---
    _vmin_label = new QLabel("Min", _scaling_section);
    _vmin_spin = new QDoubleSpinBox(_scaling_section);
    _vmin_spin->setObjectName("vmin_spin");
    _vmin_spin->setRange(-1e9, 1e9);
    _vmin_spin->setDecimals(4);
    _vmin_spin->setSingleStep(0.1);
    _vmin_spin->setValue(0.0);

    _vmax_label = new QLabel("Max", _scaling_section);
    _vmax_spin = new QDoubleSpinBox(_scaling_section);
    _vmax_spin->setObjectName("vmax_spin");
    _vmax_spin->setRange(-1e9, 1e9);
    _vmax_spin->setDecimals(4);
    _vmax_spin->setSingleStep(0.1);
    _vmax_spin->setValue(1.0);

    // --- Layout ---
    auto * form = new QFormLayout();
    form->setContentsMargins(4, 4, 4, 4);
    form->setSpacing(4);
    form->addRow("Scaling:", _scaling_combo);
    form->addRow("Color Range:", _color_range_mode_combo);
    form->addRow(_vmin_label, _vmin_spin);
    form->addRow(_vmax_label, _vmax_spin);

    _scaling_section->setContentLayout(*form);

    // Insert after the unit tree but before the spacer
    int spacer_index = ui->main_layout->indexOf(ui->vertical_spacer);
    ui->main_layout->insertWidget(spacer_index, _scaling_section);

    // --- Sync from state ---
    _syncScalingFromState();
    _updateColorRangeVisibility();

    // --- Connect signals ---
    connect(_scaling_combo, &QComboBox::currentIndexChanged,
            this, [this](int index) {
                if (!_state || index < 0) return;
                auto scaling = static_cast<ScalingMode>(
                    _scaling_combo->itemData(index).toInt());
                _state->setScaling(scaling);
            });

    connect(_color_range_mode_combo, &QComboBox::currentIndexChanged,
            this, [this](int index) {
                if (!_state || index < 0) return;
                auto mode = static_cast<HeatmapColorRangeConfig::Mode>(
                    _color_range_mode_combo->itemData(index).toInt());
                _state->setColorRangeMode(mode);
                _updateColorRangeVisibility();
            });

    connect(_vmin_spin, &QDoubleSpinBox::valueChanged,
            this, [this](double val) {
                if (!_state) return;
                _state->setColorRangeBounds(val, _vmax_spin->value());
            });

    connect(_vmax_spin, &QDoubleSpinBox::valueChanged,
            this, [this](double val) {
                if (!_state) return;
                _state->setColorRangeBounds(_vmin_spin->value(), val);
            });

    // Listen for state changes (e.g. z-score auto-switching color range mode)
    connect(_state.get(), &HeatmapState::scalingChanged,
            this, [this]() { _syncScalingFromState(); });
    connect(_state.get(), &HeatmapState::colorRangeChanged,
            this, [this]() {
                _syncScalingFromState();
                _updateColorRangeVisibility();
            });
}

void HeatmapPropertiesWidget::_syncScalingFromState()
{
    if (!_state) return;

    // Block signals to avoid feedback loops
    QSignalBlocker scaling_blocker(_scaling_combo);
    QSignalBlocker range_blocker(_color_range_mode_combo);
    QSignalBlocker vmin_blocker(_vmin_spin);
    QSignalBlocker vmax_blocker(_vmax_spin);

    // Sync scaling combo
    int scaling_idx = _scaling_combo->findData(static_cast<int>(_state->scaling()));
    if (scaling_idx >= 0) {
        _scaling_combo->setCurrentIndex(scaling_idx);
    }

    // Sync color range combo
    auto const & cr = _state->colorRange();
    int range_idx = _color_range_mode_combo->findData(static_cast<int>(cr.mode));
    if (range_idx >= 0) {
        _color_range_mode_combo->setCurrentIndex(range_idx);
    }

    // Sync spinboxes
    _vmin_spin->setValue(cr.vmin);
    _vmax_spin->setValue(cr.vmax);
}

void HeatmapPropertiesWidget::_updateColorRangeVisibility()
{
    if (!_state) return;

    bool const is_manual =
        _state->colorRange().mode == HeatmapColorRangeConfig::Mode::Manual;
    _vmin_label->setVisible(is_manual);
    _vmin_spin->setVisible(is_manual);
    _vmax_label->setVisible(is_manual);
    _vmax_spin->setVisible(is_manual);
}
