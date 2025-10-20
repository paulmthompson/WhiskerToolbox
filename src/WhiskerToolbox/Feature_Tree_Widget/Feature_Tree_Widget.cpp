#include "Feature_Tree_Widget.hpp"
#include "ui_Feature_Tree_Widget.h"

#include "../DataManager/utils/color.hpp"
#include "DataManager.hpp"

#include <QCheckBox>
#include <QHeaderView>
#include <QTreeWidgetItem>
#include <iostream>
#include <regex>

Feature_Tree_Widget::Feature_Tree_Widget(QWidget * parent)
    : QWidget(parent),
      ui(new Ui::Feature_Tree_Widget) {

    ui->setupUi(this);

    // Configure tree widget
    ui->treeWidget->setColumnCount(3);
    ui->treeWidget->setHeaderLabels(QStringList() << "Feature" << "Enabled" << "Color");
    ui->treeWidget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->treeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->treeWidget->setSortingEnabled(true);

    // Connect signals
    connect(ui->treeWidget, &QTreeWidget::itemClicked, this, &Feature_Tree_Widget::_itemSelected);
    connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &Feature_Tree_Widget::_itemChanged);
    //connect(ui->refreshButton, &QPushButton::clicked, this, &Feature_Tree_Widget::_refreshFeatures);
}

Feature_Tree_Widget::~Feature_Tree_Widget() {
    delete ui;
}

QTreeWidget * Feature_Tree_Widget::treeWidget() const { return ui->treeWidget; }

void Feature_Tree_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);

    // Register observer for data manager updates
    _data_manager->addObserver([this]() {
        _refreshFeatures();
    });

    // Initial population
    _refreshFeatures();
}

void Feature_Tree_Widget::setGroupingPattern(std::string const & pattern) {
    _grouping_pattern = pattern;
    _refreshFeatures();
}

void Feature_Tree_Widget::setTypeFilters(std::vector<DM_DataType> types) {
    _type_filters = std::move(types);
    _refreshFeatures();
}

std::vector<std::string> Feature_Tree_Widget::getSelectedFeatures() const {
    QTreeWidgetItem * item = ui->treeWidget->currentItem();
    if (!item) {
        return {};
    }

    // Get feature key
    std::string const key = item->text(0).toStdString();

    // Check if it's a group
    if (_features.find(key) != _features.end() && _features.at(key).isGroup) {
        return _features.at(key).children;
    } else {
        return {key};
    }
}

void Feature_Tree_Widget::refreshTree() {
    _refreshFeatures();
}

void Feature_Tree_Widget::_itemSelected(QTreeWidgetItem * item, int column) {

    static_cast<void>(column);

    if (_is_rebuilding) return;// Suppress selections during rebuild

    if (!item) return;

    std::string const key = item->text(0).toStdString();
    std::vector<std::string> selectedFeatures;

    // Check if it's a group
    if (_features.find(key) != _features.end()) {
        if (_features[key].isGroup || _features[key].isDataTypeGroup) {
            selectedFeatures = _features[key].children;
        } else {
            selectedFeatures = {key};
            emit featureSelected(key);// Emit single feature selection
        }
    }

    emit featuresSelected(selectedFeatures);
}

void Feature_Tree_Widget::_itemChanged(QTreeWidgetItem * item, int column) {
    if (_is_rebuilding) return;      // Suppress changes during rebuild
    if (!item || column != 1) return;// Only process checkbox column

    std::string const key = item->text(0).toStdString();
    bool const enabled{item->checkState(1) == Qt::Checked};

    // Update the feature state
    if (_features.find(key) != _features.end()) {
        _features[key].enabled = enabled;

        std::vector<std::string> affectedFeatures;

        // Handle group toggling
        if (_features[key].isGroup || _features[key].isDataTypeGroup) {
            // Only propagate to children when parent is explicitly Checked/Unchecked.
            // If parent is PartiallyChecked (typically set programmatically from child changes),
            // do not overwrite child states and do not emit group signals.
            Qt::CheckState const parentState = item->checkState(column);
            if (parentState == Qt::Checked || parentState == Qt::Unchecked) {
                // Block signals while updating children to avoid per-child emissions
                if (ui && ui->treeWidget) ui->treeWidget->blockSignals(true);
                _updateChildrenState(item, column);
                if (ui && ui->treeWidget) ui->treeWidget->blockSignals(false);

                // Add all children to affected features
                affectedFeatures = _features[key].children;

                // Emit signals for multiple features only once per group toggle
                if (parentState == Qt::Checked) {
                    emit addFeatures(affectedFeatures);
                } else {
                    emit removeFeatures(affectedFeatures);
                }
            }
        } else {
            affectedFeatures = {key};

            // Update parent state if needed
            _updateParentState(item, column);

            // Emit single feature signals
            if (enabled) {
                emit addFeature(key);
            } else {
                emit removeFeature(key);
            }
        }
    }
}

void Feature_Tree_Widget::_refreshFeatures() {
    // Save current state before rebuilding
    _is_rebuilding = true;// Guard emissions
    _saveCurrentState();

    // Clear existing data
    ui->treeWidget->clear();
    _feature_items.clear();
    _group_items.clear();
    _datatype_items.clear();
    _features.clear();

    // Populate tree
    _populateTree();

    // Restore state after rebuilding
    _restoreState();
    _is_rebuilding = false;// End guard
}

void Feature_Tree_Widget::_populateTree() {

    if (!_data_manager) {
        return;
    }

    // Get all keys
    auto allKeys = _data_manager->getAllKeys();

    if (_organize_by_datatype) {
        _populateTreeByDataType(allKeys);
    } else {
        _populateTreeFlat(allKeys);
    }
}

void Feature_Tree_Widget::_populateTreeByDataType(std::vector<std::string> const & allKeys) {

    if (!_data_manager) {
        return;
    }

    // Organize by data type first
    std::unordered_map<DM_DataType, std::vector<std::string>> dataTypeGroups;

    for (auto const & key: allKeys) {
        auto const type = _data_manager->getType(key);
        if (!_type_filters.empty() && !_hasTypeFilter(type)) {
            continue;
        }
        dataTypeGroups[type].push_back(key);
    }

    // Create data type groups and organize within each
    for (auto const & [dataType, keys]: dataTypeGroups) {
        if (keys.empty()) continue;

        // Create data type group item
        QTreeWidgetItem * dataTypeItem = _getOrCreateDataTypeItem(dataType);

        // Within each data type, group by underscore pattern
        std::unordered_map<std::string, std::vector<std::string>> nameGroups;

        for (auto const & key: keys) {
            std::string const groupName = _extractGroupName(key);
            if (!groupName.empty() && groupName != key) {
                nameGroups[groupName].push_back(key);
            }
        }

        // Add grouped items
        for (auto const & [groupName, members]: nameGroups) {
            if (members.size() > 1) {
                // Create a name group under the data type
                TreeFeature groupFeature;
                groupFeature.key = groupName;
                groupFeature.type = "Group";
                groupFeature.isGroup = true;
                groupFeature.isDataTypeGroup = false;
                groupFeature.children = members;
                groupFeature.dataType = dataType;
                _features[groupName] = groupFeature;

                auto * groupItem = new QTreeWidgetItem(dataTypeItem);
                groupItem->setText(0, QString::fromStdString(groupName));
                groupItem->setFlags(groupItem->flags() | Qt::ItemIsUserCheckable);
                setup_checkbox_column(groupItem, 1, false);
                _group_items[groupName] = groupItem;

                // Add children
                for (auto const & member: members) {
                    TreeFeature childFeature;
                    childFeature.key = member;
                    childFeature.type = convert_data_type_to_string(dataType);
                    childFeature.timeFrame = _data_manager->getTimeKey(member).str();
                    childFeature.isGroup = false;
                    childFeature.isDataTypeGroup = false;
                    childFeature.dataType = dataType;
                    _features[member] = childFeature;

                    auto * childItem = new QTreeWidgetItem(groupItem);
                    childItem->setText(0, QString::fromStdString(member));
                    childItem->setFlags(childItem->flags() | Qt::ItemIsUserCheckable);
                    setup_checkbox_column(childItem, 1, false);
                    _feature_items[member] = childItem;
                }
            }
        }

        // Add standalone items
        for (auto const & key: keys) {
            bool inGroup = false;
            for (auto const & [groupName, members]: nameGroups) {
                if (members.size() > 1 && std::find(members.begin(), members.end(), key) != members.end()) {
                    inGroup = true;
                    break;
                }
            }

            if (!inGroup && _features.find(key) == _features.end()) {
                TreeFeature feature;
                feature.key = key;
                feature.type = convert_data_type_to_string(dataType);
                feature.timeFrame = _data_manager->getTimeKey(key).str();
                feature.isGroup = false;
                feature.isDataTypeGroup = false;
                feature.dataType = dataType;
                _features[key] = feature;

                auto * item = new QTreeWidgetItem(dataTypeItem);
                item->setText(0, QString::fromStdString(key));
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                setup_checkbox_column(item, 1, false);
                _feature_items[key] = item;
            }
        }
    }

    ui->treeWidget->expandAll();
}

void Feature_Tree_Widget::_populateTreeFlat(std::vector<std::string> const & allKeys) {

    if (!_data_manager) {
        return;
    }

    // First pass: identify groups
    std::unordered_map<std::string, std::vector<std::string>> groups;

    for (auto const & key: allKeys) {
        // Skip if type doesn't match filter
        auto const type = _data_manager->getType(key);
        if (!_type_filters.empty() && !_hasTypeFilter(type)) {
            continue;
        }

        // Try to extract group name
        std::string const groupName = _extractGroupName(key);

        if (!groupName.empty() && groupName != key) {
            // This is part of a group
            groups[groupName].push_back(key);
        }
    }

    // Second pass: add groups and standalone items
    for (auto const & [groupName, members]: groups) {
        if (members.size() > 1) {
            // Create group in features
            TreeFeature groupFeature;
            groupFeature.key = groupName;
            groupFeature.type = "Group";
            groupFeature.isGroup = true;
            groupFeature.children = members;
            _features[groupName] = groupFeature;

            // Create tree item
            _addFeatureToTree(groupName, true);

            // Add children to the group
            for (auto const & member: members) {
                // Create feature
                TreeFeature childFeature;
                childFeature.key = member;
                childFeature.type = convert_data_type_to_string(_data_manager->getType(member));
                childFeature.timeFrame = _data_manager->getTimeKey(member).str();
                childFeature.isGroup = false;
                _features[member] = childFeature;

                // Add as child to tree
                QTreeWidgetItem * groupItem = _group_items[groupName];
                auto * childItem = new QTreeWidgetItem(groupItem);
                childItem->setText(0, QString::fromStdString(member));
                childItem->setFlags(childItem->flags() | Qt::ItemIsUserCheckable);

                setup_checkbox_column(childItem, 1, false);

                _feature_items[member] = childItem;
            }
        }
    }

    // Add standalone items (not in any group)
    for (auto const & key: allKeys) {
        // Skip if type doesn't match filter
        auto const type = _data_manager->getType(key);
        if (!_type_filters.empty() && !_hasTypeFilter(type)) {
            continue;
        }

        // Skip if already added as part of a group
        bool inGroup = false;
        for (auto const & [groupName, members]: groups) {
            if (members.size() > 1 && std::find(members.begin(), members.end(), key) != members.end()) {
                inGroup = true;
                break;
            }
        }

        if (!inGroup && _features.find(key) == _features.end()) {
            // Create feature
            TreeFeature feature;
            feature.key = key;
            feature.type = convert_data_type_to_string(type);
            feature.timeFrame = _data_manager->getTimeKey(key).str();
            feature.isGroup = false;
            _features[key] = feature;

            // Add to tree
            _addFeatureToTree(key);
        }
    }

    // Expand all items for better visibility
    ui->treeWidget->expandAll();
}

std::string Feature_Tree_Widget::_extractGroupName(std::string const & key) {
    std::regex const pattern{_grouping_pattern};
    std::smatch matches{};

    if (std::regex_search(key, matches, pattern) && matches.size() > 1) {
        return matches[1].str();
    }

    return key;// Return the key itself if no match
}

std::vector<std::string> get_child_features(QTreeWidgetItem * item) {
    std::vector<std::string> children;
    if (!item) return children;

    for (int i = 0; i < item->childCount(); i++) {
        QTreeWidgetItem * child = item->child(i);
        children.push_back(child->text(0).toStdString());
    }

    return children;
}

void Feature_Tree_Widget::_addFeatureToTree(std::string const & key, bool isGroup, bool isDataTypeGroup) {
    if (_features.find(key) == _features.end()) {
        return;
    }

    auto * item = new QTreeWidgetItem(ui->treeWidget);
    item->setText(0, QString::fromStdString(key));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

    setup_checkbox_column(item, 1, false);

    if (isDataTypeGroup) {
        _datatype_items[key] = item;
    } else if (isGroup) {
        _group_items[key] = item;
    } else {
        _feature_items[key] = item;
    }
}

void Feature_Tree_Widget::_setupTreeItem(QTreeWidgetItem * item, TreeFeature const & feature) {
    item->setText(0, QString::fromStdString(feature.key));

    setup_checkbox_column(item, 1, feature.enabled);
}

void setup_checkbox_column(QTreeWidgetItem * item, int column, bool checked) {
    item->setCheckState(column, checked ? Qt::Checked : Qt::Unchecked);
}

bool Feature_Tree_Widget::_hasTypeFilter(DM_DataType const & type) {
    return std::find(_type_filters.begin(), _type_filters.end(), type) != _type_filters.end();
}

void Feature_Tree_Widget::_updateChildrenState(QTreeWidgetItem * parent, int column) {
    if (!parent) return;

    Qt::CheckState const parentState = parent->checkState(column);

    // Update all children with parent's state
    for (int i = 0; i < parent->childCount(); i++) {
        QTreeWidgetItem * child = parent->child(i);
        child->setCheckState(column, parentState);

        // Update feature state
        std::string const childKey = child->text(0).toStdString();
        if (_features.find(childKey) != _features.end()) {
            _features[childKey].enabled = (parentState == Qt::Checked);
        }
    }
}

void Feature_Tree_Widget::_updateParentState(QTreeWidgetItem * child, int column) {
    QTreeWidgetItem * parent = child->parent();
    if (!parent) return;

    bool allChecked = true;
    bool allUnchecked = true;

    // Check all siblings
    for (int i = 0; i < parent->childCount(); i++) {
        QTreeWidgetItem * sibling = parent->child(i);
        if (sibling->checkState(column) == Qt::Checked) {
            allUnchecked = false;
        } else {
            allChecked = false;
        }
    }

    // Set parent state based on children
    if (allChecked) {
        parent->setCheckState(column, Qt::Checked);
    } else if (allUnchecked) {
        parent->setCheckState(column, Qt::Unchecked);
    } else {
        parent->setCheckState(column, Qt::PartiallyChecked);
    }

    // Update feature state
    std::string const parentKey = parent->text(0).toStdString();
    if (_features.find(parentKey) != _features.end()) {
        _features[parentKey].enabled = (parent->checkState(column) == Qt::Checked);
    }
}

QTreeWidgetItem * Feature_Tree_Widget::_getOrCreateDataTypeItem(DM_DataType dataType) {
    std::string const dataTypeName = _getDataTypeGroupName(dataType);

    if (_datatype_items.find(dataTypeName) != _datatype_items.end()) {
        return _datatype_items[dataTypeName];
    }

    // Create new data type group
    TreeFeature dataTypeFeature;
    dataTypeFeature.key = dataTypeName;
    dataTypeFeature.type = "Data Type";
    dataTypeFeature.isGroup = true;
    dataTypeFeature.isDataTypeGroup = true;
    dataTypeFeature.dataType = dataType;
    _features[dataTypeName] = dataTypeFeature;

    auto * item = new QTreeWidgetItem(ui->treeWidget);
    item->setText(0, QString::fromStdString(dataTypeName));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    setup_checkbox_column(item, 1, false);

    _datatype_items[dataTypeName] = item;
    return item;
}

std::string Feature_Tree_Widget::_getDataTypeGroupName(DM_DataType dataType) {
    return convert_data_type_to_string(dataType);
}

std::string Feature_Tree_Widget::getSelectedFeature() const {
    QTreeWidgetItem * item = ui->treeWidget->currentItem();
    if (!item) {
        return "";
    }
    return item->text(0).toStdString();
}

void Feature_Tree_Widget::_saveCurrentState() {
    // Clear previous state
    _enabled_features.clear();
    _expanded_groups.clear();
    _selected_feature_for_restoration.clear();

    // Save currently selected feature
    QTreeWidgetItem * currentItem = ui->treeWidget->currentItem();
    if (currentItem) {
        _selected_feature_for_restoration = currentItem->text(0).toStdString();
    }

    // Helper function to recursively save state for all items
    std::function<void(QTreeWidgetItem *)> saveItemState = [&](QTreeWidgetItem * item) {
        if (!item) return;

        std::string const itemKey = item->text(0).toStdString();

        // Save enabled state (checkbox in column 1)
        if (item->checkState(1) == Qt::Checked) {
            _enabled_features.insert(itemKey);
        }

        // Save expanded state
        if (item->isExpanded()) {
            _expanded_groups.insert(itemKey);
        }

        // Recursively save child states
        for (int i = 0; i < item->childCount(); ++i) {
            saveItemState(item->child(i));
        }
    };

    // Save state for all top-level items
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        saveItemState(ui->treeWidget->topLevelItem(i));
    }
}

void Feature_Tree_Widget::_restoreState() {
    // Block signals during state restoration to avoid triggering itemChanged
    ui->treeWidget->blockSignals(true);

    // Helper function to recursively restore state for all items
    std::function<void(QTreeWidgetItem *)> restoreItemState = [&](QTreeWidgetItem * item) {
        if (!item) return;

        std::string const itemKey = item->text(0).toStdString();

        // Restore enabled state (checkbox in column 1)
        bool const shouldBeEnabled = _enabled_features.find(itemKey) != _enabled_features.end();
        item->setCheckState(1, shouldBeEnabled ? Qt::Checked : Qt::Unchecked);

        // Update the feature state in our internal tracking
        if (_features.find(itemKey) != _features.end()) {
            _features[itemKey].enabled = shouldBeEnabled;
        }

        // Restore expanded state
        bool const shouldBeExpanded = _expanded_groups.find(itemKey) != _expanded_groups.end();
        item->setExpanded(shouldBeExpanded);

        // Restore selection state
        if (itemKey == _selected_feature_for_restoration) {
            ui->treeWidget->setCurrentItem(item);
        }

        // Recursively restore child states
        for (int i = 0; i < item->childCount(); ++i) {
            restoreItemState(item->child(i));
        }
    };

    // Restore state for all top-level items
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); ++i) {
        restoreItemState(ui->treeWidget->topLevelItem(i));
    }

    // Unblock signals after restoration is complete
    ui->treeWidget->blockSignals(false);
}
