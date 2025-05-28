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
    _data_manager = std::move(data_manager);
}

void DataViewer_Tree_Widget::setTypeFilter(std::vector<DM_DataType> const & types) {
    _type_filter = types;
}

void DataViewer_Tree_Widget::populateTree() {
    if (!_data_manager) {
        return;
    }
    
    // Clear existing items
    clear();
    _groups.clear();
    
    // Create groups based on data in DataManager
    _createGroups();
    
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
    for (auto const & [group_key, group] : _groups) {
        if (group->series_keys.size() == 1) {
            // Single item group - display as flat entry without hierarchy
            auto* series_item = new QTreeWidgetItem(this);
            std::string const & series_key = group->series_keys[0];
            series_item->setText(0, QString::fromStdString(series_key));
            series_item->setText(1, _getDataTypeString(group->data_type));
            series_item->setFlags(series_item->flags() | Qt::ItemIsUserCheckable);
            series_item->setCheckState(0, Qt::Unchecked);
            
            // Store series key as data for flat entries
            series_item->setData(0, Qt::UserRole, QString::fromStdString(series_key));
            
            // Store reference to this item as the group's tree item for consistency
            group->tree_item = series_item;
            
        } else {
            // Multiple items - create hierarchical group
            auto* group_item = new QTreeWidgetItem(this);
            group_item->setText(0, QString::fromStdString(group->prefix));
            group_item->setText(1, _getDataTypeString(group->data_type));
            group_item->setFlags(group_item->flags() | Qt::ItemIsUserCheckable);
            group_item->setCheckState(0, Qt::Unchecked);
            
            // Make group item bold
            QFont font = group_item->font(0);
            font.setBold(true);
            group_item->setFont(0, font);
            group_item->setFont(1, font);
            
            // Store reference
            group->tree_item = group_item;
            
            // Add child items for each series
            for (auto const & series_key : group->series_keys) {
                auto* series_item = new QTreeWidgetItem(group_item);
                series_item->setText(0, QString::fromStdString(series_key));
                series_item->setText(1, _getDataTypeString(group->data_type));
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
        auto group = std::make_unique<SeriesGroup>();
        group->prefix = prefix;
        group->data_type = data_type;
        group->series_keys.push_back(series_key);
        _groups[group_key] = std::move(group);
    } else {
        it->second->series_keys.push_back(series_key);
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
    if (!item) {
        std::cerr << "Error: null item in _onItemChanged" << std::endl;
        return;
    }
    
    if (column != 0) return; // Only handle checkbox changes in first column
    
    // Prevent recursion using flag instead of blocking all signals
    if (_updating_items) {
        std::cout << "Skipping item change due to recursion prevention" << std::endl;
        return;
    }
    
    if (item->parent() == nullptr) {
        // This is a top-level item - could be a group or a flat single-item entry
        QVariant userData = item->data(0, Qt::UserRole);
        
        if (userData.isValid()) {
            // This is a flat single-item entry (has user data)
            std::string series_key = userData.toString().toStdString();
            bool enabled = (item->checkState(0) == Qt::Checked);
            
            std::cout << "Flat series item changed: " << series_key << " enabled: " << enabled << std::endl;
            std::cout << "About to emit seriesToggled signal..." << std::endl;
            
            emit seriesToggled(QString::fromStdString(series_key), enabled);
            
            std::cout << "seriesToggled signal emitted successfully" << std::endl;
            
        } else {
            // This is a group item (no user data)
            bool enabled = (item->checkState(0) == Qt::Checked);
            
            // Find the group
            std::string group_prefix = item->text(0).toStdString();
            std::cout << "Group item changed: " << group_prefix << " enabled: " << enabled << std::endl;
            
            for (auto const & [group_key, group] : _groups) {
                if (group && group->tree_item == item) {
                    _setGroupEnabled(group.get(), enabled);
                    emit groupToggled(QString::fromStdString(group->prefix), group->data_type, enabled);
                    break;
                }
            }
        }
    } else {
        // This is a series item within a multi-item group
        QVariant userData = item->data(0, Qt::UserRole);
        if (!userData.isValid()) {
            std::cerr << "Error: series item has no user data" << std::endl;
            return;
        }
        
        std::string series_key = userData.toString().toStdString();
        bool enabled = (item->checkState(0) == Qt::Checked);
        
        std::cout << "Series item changed: " << series_key << " enabled: " << enabled << std::endl;
        std::cout << "About to emit seriesToggled signal..." << std::endl;
        
        emit seriesToggled(QString::fromStdString(series_key), enabled);
        
        std::cout << "seriesToggled signal emitted successfully" << std::endl;
        
        // Update parent group check state
        auto* parent_item = item->parent();
        if (parent_item) {
            _updating_items = true; // Prevent recursion when updating group state
            for (auto const & [group_key, group] : _groups) {
                if (group && group->tree_item == parent_item) {
                    _updateGroupCheckState(group.get());
                    break;
                }
            }
            _updating_items = false;
        }
    }
}

void DataViewer_Tree_Widget::_onItemClicked(QTreeWidgetItem* item, int column) {
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
    
    int checked_count = 0;
    int total_count = group->tree_item->childCount();
    
    for (int i = 0; i < total_count; ++i) {
        auto* child = group->tree_item->child(i);
        if (child->checkState(0) == Qt::Checked) {
            checked_count++;
        }
    }
    
    if (checked_count == 0) {
        group->tree_item->setCheckState(0, Qt::Unchecked);
        group->all_enabled = false;
    } else if (checked_count == total_count) {
        group->tree_item->setCheckState(0, Qt::Checked);
        group->all_enabled = true;
    } else {
        group->tree_item->setCheckState(0, Qt::PartiallyChecked);
        group->all_enabled = false;
    }
}

void DataViewer_Tree_Widget::_setGroupEnabled(SeriesGroup* group, bool enabled) {
    if (!group->tree_item) return;
    
    group->all_enabled = enabled;
    
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