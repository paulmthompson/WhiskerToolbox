#include "HeatmapPropertiesWidget.hpp"

#include "Collapsible_Widget/Section.hpp"
#include "Core/HeatmapState.hpp"
#include "DataManager/DataManager.hpp"
#include "Feature_Tree_Widget/Feature_Tree_Widget.hpp"
#include "Plots/Common/PlotAlignmentWidget/UI/PlotAlignmentWidget.hpp"
#include "Plots/Common/RelativeTimeAxisWidget/RelativeTimeAxisWithRangeControls.hpp"
#include "Plots/Common/VerticalAxisWidget/VerticalAxisWithRangeControls.hpp"
#include "UI/HeatmapWidget.hpp"

#include "ui_HeatmapPropertiesWidget.h"

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
