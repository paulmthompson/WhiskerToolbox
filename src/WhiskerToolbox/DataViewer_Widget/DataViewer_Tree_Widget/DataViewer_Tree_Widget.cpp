#include "DataViewer_Tree_Widget.hpp"

#include "DataManager/DataManager.hpp"
#include "DataManager/DataManagerTypes.hpp"

#include <QHeaderView>
#include <QFont>

#include <algorithm>
#include <iostream>
#include <regex>

DataViewer_Tree_Widget::DataViewer_Tree_Widget(QWidget* parent)
    : QTreeWidget(parent) {
    
    // Setup tree widget
    setHeaderLabels({"Series", "Type"});
    setColumnCount(2);
    
    // Configure appearance
    setAlternatingRowColors(true);
    setRootIsDecorated(true);
    setItemsExpandable(true);
    setExpandsOnDoubleClick(true);
    
    // Enable checkboxes
    setSelectionMode(QAbstractItemView::SingleSelection);
    
    // Connect signals
    connect(this, &QTreeWidget::itemChanged, this, &DataViewer_Tree_Widget::_onItemChanged);
    connect(this, &QTreeWidget::itemClicked, this, &DataViewer_Tree_Widget::_onItemClicked);
    
    // Set column widths
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
}

DataViewer_Tree_Widget::~DataViewer_Tree_Widget() = default;

void DataViewer_Tree_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    std::cout << "DataViewer_Tree_Widget: setDataManager called" << std::endl;
    _data_manager = std::move(data_manager);
    
    // Set up auto-population observer
    _data_manager->addObserver([this]() {
        std::cout << "DataViewer_Tree_Widget: Observer callback triggered" << std::endl;
        _autoPopulateTree();
    });
    
    // Initial population
    std::cout << "DataViewer_Tree_Widget: Triggering initial population" << std::endl;
    _autoPopulateTree();
}

void DataViewer_Tree_Widget::setTypeFilter(std::vector<DM_DataType> const & types) {
    _type_filter = types;
}

void DataViewer_Tree_Widget::populateTree() {
    if (!_data_manager) {
        return;
    }
    
    // Use the auto-populate method which handles state preservation
    _autoPopulateTree();
    
    // Expand all groups by default
    expandAll();
}

void DataViewer_Tree_Widget::_createGroups() {
    // Get all data keys from DataManager
    auto all_keys = _data_manager->getAllKeys();
    
    // Filter keys by type and group them
    for (auto const & key : all_keys) {
        auto data_type = _data_manager->getType(key);
        
        // Skip if not in type filter
        if (!_type_filter.empty()) {
            auto it = std::find(_type_filter.begin(), _type_filter.end(), data_type);
            if (it == _type_filter.end()) {
                continue;
            }
        }
        
        _addSeriesToGroup(key, data_type);
    }
    
    // Create tree items for each group
    for (auto & [group_key, group] : _groups) {
        if (group.series_keys.size() == 1) {
            // Single item group - display as flat entry without hierarchy
            auto* series_item = new QTreeWidgetItem(this);
            std::string const & series_key = group.series_keys[0];
            series_item->setText(0, QString::fromStdString(series_key));
            series_item->setText(1, _getDataTypeString(group.data_type));
            series_item->setFlags(series_item->flags() | Qt::ItemIsUserCheckable);
            series_item->setCheckState(0, Qt::Unchecked);
            
            // Store series key as data for flat entries
            series_item->setData(0, Qt::UserRole, QString::fromStdString(series_key));
            
            // Store reference to this item as the group's tree item for consistency
            group.tree_item = series_item;
            
        } else {
            // Multiple items - create hierarchical group
            auto* group_item = new QTreeWidgetItem(this);
            group_item->setText(0, QString::fromStdString(group.prefix));
            group_item->setText(1, _getDataTypeString(group.data_type));
            group_item->setFlags(group_item->flags() | Qt::ItemIsUserCheckable);
            group_item->setCheckState(0, Qt::Unchecked);
            
            // Make group item bold
            QFont font = group_item->font(0);
            font.setBold(true);
            group_item->setFont(0, font);
            group_item->setFont(1, font);
            
            // Store reference
            group.tree_item = group_item;
            
            // Add child items for each series
            for (auto const & series_key : group.series_keys) {
                auto* series_item = new QTreeWidgetItem(group_item);
                series_item->setText(0, QString::fromStdString(series_key));
                series_item->setText(1, _getDataTypeString(group.data_type));
                series_item->setFlags(series_item->flags() | Qt::ItemIsUserCheckable);
                series_item->setCheckState(0, Qt::Unchecked);
                
                // Store series key as data
                series_item->setData(0, Qt::UserRole, QString::fromStdString(series_key));
            }
        }
    }
}

void DataViewer_Tree_Widget::_addSeriesToGroup(std::string const & series_key, DM_DataType data_type) {
    std::string prefix = _extractPrefix(series_key);
    std::string group_key = prefix + "_" + convert_data_type_to_string(data_type);
    
    // Find or create group
    auto it = _groups.find(group_key);
    if (it == _groups.end()) {
        SeriesGroup group;
        group.prefix = prefix;
        group.data_type = data_type;
        group.series_keys.push_back(series_key);
        _groups[group_key] = std::move(group);
    } else {
        it->second.series_keys.push_back(series_key);
    }
}

std::string DataViewer_Tree_Widget::_extractPrefix(std::string const & series_key) {
    // Look for common patterns like "channel_1", "sensor_A", "data_001", etc.
    std::regex pattern(R"(^(.+?)(_\d+|_[A-Za-z]+|\d+)$)");
    std::smatch match;
    
    if (std::regex_match(series_key, match, pattern)) {
        return match[1].str();
    }
    
    // If no pattern found, use the whole key as prefix
    return series_key;
}

void DataViewer_Tree_Widget::_onItemChanged(QTreeWidgetItem* item, int column) {
    if (_updating_items || column != 0) {
        return;
    }
    
    bool is_checked = (item->checkState(0) == Qt::Checked);
    
    // Check if this item has UserRole data (series item) or not (group item)
    QVariant userData = item->data(0, Qt::UserRole);
    
    _updating_items = true;
    
    if (!userData.isValid()) {
        // This is a group item (no UserRole data)
        std::string group_name = item->text(0).toStdString();
        
        // Find the corresponding group and set all series in it
        for (auto & [group_key, group] : _groups) {
            if (group.tree_item == item) {
                _setGroupEnabled(&group, is_checked);
                
                emit groupToggled(QString::fromStdString(group.prefix), group.data_type, is_checked);
                break;
            }
        }
    } else {
        // This is a series item (has UserRole data)
        std::string series_key = userData.toString().toStdString();
        
        // Update parent group state if this is within a group
        QTreeWidgetItem* parent_item = item->parent();
        if (parent_item) {
            // Find the corresponding group
            for (auto & [group_key, group] : _groups) {
                if (group.tree_item == parent_item) {
                    _updateGroupCheckState(&group);
                    break;
                }
            }
        }
        
        emit seriesToggled(QString::fromStdString(series_key), is_checked);
    }
    
    _updating_items = false;
}

void DataViewer_Tree_Widget::_onItemClicked(QTreeWidgetItem* item, int column) {

    static_cast<void>(column);

    if (!item) {
        std::cerr << "Error: null item in _onItemClicked" << std::endl;
        return;
    }
    
    // Check if this item has user data (meaning it's a series item)
    QVariant userData = item->data(0, Qt::UserRole);
    if (userData.isValid()) {
        // This is a series item (either flat or within a group) - emit selection signal
        std::string series_key = userData.toString().toStdString();
        std::cout << "Series selected: " << series_key << std::endl;
        emit seriesSelected(QString::fromStdString(series_key));
    }
    // If no user data, it's a group header - no selection signal needed
}

void DataViewer_Tree_Widget::_updateGroupCheckState(SeriesGroup* group) {
    if (!group->tree_item) return;
    
    // Check how many children are checked
    int checked_count = 0;
    int total_count = group->tree_item->childCount();
    
    for (int i = 0; i < total_count; ++i) {
        if (group->tree_item->child(i)->checkState(0) == Qt::Checked) {
            checked_count++;
        }
    }
    
    // Update group checkbox state based on children
    if (checked_count == 0) {
        group->tree_item->setCheckState(0, Qt::Unchecked);
    } else if (checked_count == total_count) {
        group->tree_item->setCheckState(0, Qt::Checked);
    } else {
        group->tree_item->setCheckState(0, Qt::PartiallyChecked);
    }
}

void DataViewer_Tree_Widget::_setGroupEnabled(SeriesGroup* group, bool enabled) {
    if (!group->tree_item) return;
    
    // Set flag to prevent recursion when we change child states
    _updating_items = true;
    
    if (group->series_keys.size() == 1) {
        // Single item group - the tree_item IS the series item
        // Just emit the signal, the checkbox state is already set by the user click
        std::string const & series_key = group->series_keys[0];
        std::cout << "Emitting seriesToggled for single-item group: " << series_key << " enabled: " << enabled << std::endl;
        emit seriesToggled(QString::fromStdString(series_key), enabled);
    } else {
        // Multiple items - set all children to the same state
        for (int i = 0; i < group->tree_item->childCount(); ++i) {
            auto* child = group->tree_item->child(i);
            child->setCheckState(0, enabled ? Qt::Checked : Qt::Unchecked);
            
            // Emit signal for each series (this won't trigger _onItemChanged due to flag)
            std::string series_key = child->data(0, Qt::UserRole).toString().toStdString();
            std::cout << "Emitting seriesToggled for group child: " << series_key << " enabled: " << enabled << std::endl;
            emit seriesToggled(QString::fromStdString(series_key), enabled);
        }
    }
    
    _updating_items = false;
}

QString DataViewer_Tree_Widget::_getDataTypeString(DM_DataType type) const {
    switch (type) {
        case DM_DataType::Analog:
            return "Analog";
        case DM_DataType::DigitalEvent:
            return "Digital Event";
        case DM_DataType::DigitalInterval:
            return "Digital Interval";
        case DM_DataType::Points:
            return "Points";
        case DM_DataType::Line:
            return "Line";
        case DM_DataType::Mask:
            return "Mask";
        case DM_DataType::Tensor:
            return "Tensor";
        case DM_DataType::Video:
            return "Video";
        case DM_DataType::Time:
            return "Time";
        default:
            return "Unknown";
    }
}

void DataViewer_Tree_Widget::_autoPopulateTree() {
    std::cout << "DataViewer_Tree_Widget: _autoPopulateTree called" << std::endl;
    
    if (!_data_manager) {
        std::cout << "DataViewer_Tree_Widget: No data manager, aborting auto-populate" << std::endl;
        return;
    }
    
    // Save current state BEFORE clearing anything
    _saveCurrentState();
    
    // Block signals to prevent unwanted itemChanged emissions during tree clearing and restoration
    std::cout << "DataViewer_Tree_Widget: Blocking signals during repopulation" << std::endl;
    blockSignals(true);
    
    // Clear existing tree and groups AFTER saving state
    clear();
    _groups.clear();
    
    // Repopulate with all available data
    _createGroups();
    
    // Restore previous state
    _restoreState();
    
    // Re-enable signals after restoration is complete
    std::cout << "DataViewer_Tree_Widget: Re-enabling signals after repopulation" << std::endl;
    blockSignals(false);
    
    // Expand all groups by default (like in populateTree)
    expandAll();
    
    std::cout << "DataViewer_Tree_Widget: _autoPopulateTree completed" << std::endl;
}

void DataViewer_Tree_Widget::_saveCurrentState() {
    std::cout << "DataViewer_Tree_Widget: Saving current state..." << std::endl;
    
    // Collect enabled series and group states from current tree
    std::unordered_set<std::string> enabled_series;
    std::unordered_map<std::string, bool> group_states;
    
    // Iterate through all top-level items
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = topLevelItem(i);
        
        // Check if this is a flat series item (has UserRole data)
        QVariant userData = item->data(0, Qt::UserRole);
        if (userData.isValid()) {
            // This is a flat series item
            std::string series_key = userData.toString().toStdString();
            if (item->checkState(0) == Qt::Checked) {
                enabled_series.insert(series_key);
                std::cout << "  Saved enabled flat series: " << series_key << std::endl;
            }
        } else {
            // This is a group item
            std::string group_name = item->text(0).toStdString();
            bool is_checked = (item->checkState(0) == Qt::Checked);
            group_states[group_name] = is_checked;
            std::cout << "  Saved group state: " << group_name << " = " << (is_checked ? "enabled" : "disabled") << std::endl;
            
            // Also save individual series within the group
            for (int j = 0; j < item->childCount(); ++j) {
                QTreeWidgetItem* child = item->child(j);
                QVariant childUserData = child->data(0, Qt::UserRole);
                if (childUserData.isValid()) {
                    std::string series_key = childUserData.toString().toStdString();
                    if (child->checkState(0) == Qt::Checked) {
                        enabled_series.insert(series_key);
                        std::cout << "  Saved enabled grouped series: " << series_key << std::endl;
                    }
                }
            }
        }
    }
    
    // Save to state manager
    _state_manager.saveEnabledSeries(enabled_series);
    _state_manager.saveGroupStates(group_states);
    
    std::cout << "DataViewer_Tree_Widget: Saved " << enabled_series.size() << " enabled series and " 
              << group_states.size() << " group states" << std::endl;
}

void DataViewer_Tree_Widget::_restoreState() {
    std::cout << "DataViewer_Tree_Widget: Restoring state..." << std::endl;
    
    _updating_items = true; // Prevent signal emission during restoration
    
    // Restore states for all items using TreeWidgetStateManager
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = topLevelItem(i);
        
        // Check if this is a flat series item (has UserRole data)
        QVariant userData = item->data(0, Qt::UserRole);
        if (userData.isValid()) {
            // This is a flat series item
            std::string series_key = userData.toString().toStdString();
            bool should_enable = _state_manager.shouldSeriesBeEnabled(series_key);
            item->setCheckState(0, should_enable ? Qt::Checked : Qt::Unchecked);
            
            if (should_enable) {
                std::cout << "  Restored flat series: " << series_key << " (enabled)" << std::endl;
            }
        } else {
            // This is a group item
            std::string group_name = item->text(0).toStdString();
            auto group_state = _state_manager.shouldGroupBeEnabled(group_name);
            
            if (group_state.has_value()) {
                item->setCheckState(0, group_state.value() ? Qt::Checked : Qt::Unchecked);
                std::cout << "  Restored group: " << group_name << " (" 
                          << (group_state.value() ? "enabled" : "disabled") << ")" << std::endl;
            }
            
            // Restore states for child series items
            for (int j = 0; j < item->childCount(); ++j) {
                QTreeWidgetItem* child = item->child(j);
                QVariant childUserData = child->data(0, Qt::UserRole);
                if (childUserData.isValid()) {
                    std::string series_key = childUserData.toString().toStdString();
                    bool should_enable = _state_manager.shouldSeriesBeEnabled(series_key);
                    child->setCheckState(0, should_enable ? Qt::Checked : Qt::Unchecked);
                    
                    if (should_enable) {
                        std::cout << "  Restored grouped series: " << series_key << " (enabled)" << std::endl;
                    }
                }
            }
        }
    }
    
    _updating_items = false;
    std::cout << "DataViewer_Tree_Widget: State restoration complete" << std::endl;
} 
