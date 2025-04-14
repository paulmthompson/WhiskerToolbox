#include "Feature_Tree_Widget.hpp"
#include "ui_Feature_Tree_Widget.h"

#include "Color_Widget/Color_Widget.hpp"
#include "DataManager.hpp"
#include "utils/color.hpp"

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
    ui->treeWidget->setColumnCount(4);
    ui->treeWidget->setHeaderLabels(QStringList() << "Feature" << "Type" << "Enabled" << "Color");
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

void Feature_Tree_Widget::setDataManager(std::shared_ptr<DataManager> data_manager) {
    _data_manager = std::move(data_manager);

    // Register observer for data manager updates
    _data_manager->addObserver([this]() {
        _refreshFeatures();
    });

    // Initial population
    _refreshFeatures();
}

void Feature_Tree_Widget::setGroupingPattern(std::string pattern) {
    _grouping_pattern = std::move(pattern);
    _refreshFeatures();
}

void Feature_Tree_Widget::setTypeFilters(std::vector<std::string> types) {
    _type_filters = std::move(types);
    _refreshFeatures();
}

std::vector<std::string> Feature_Tree_Widget::getSelectedFeatures() const {
    QTreeWidgetItem * item = ui->treeWidget->currentItem();
    if (!item) {
        return {};
    }

    // Get feature key
    std::string key = item->text(0).toStdString();

    // Check if it's a group
    if (_features.find(key) != _features.end() && _features.at(key).isGroup) {
        return _features.at(key).children;
    } else {
        return {key};
    }
}

std::string Feature_Tree_Widget::getFeatureColor(std::string const & key) {
    if (_features.find(key) != _features.end()) {
        return _features[key].color;
    }
    return "";
}

void Feature_Tree_Widget::setFeatureColor(std::string const & key, std::string const & hex_color) {
    if (_features.find(key) == _features.end()) {
        return;
    }

    _features[key].color = hex_color;

    // Update UI if item exists
    if (_feature_items.find(key) != _feature_items.end()) {
        QTreeWidgetItem * item = _feature_items[key];
        _setupColorColumn(item, 3, hex_color);
    }
}

std::string Feature_Tree_Widget::getGroupColor(std::string const & group) {
    return getFeatureColor(group);
}

void Feature_Tree_Widget::setGroupColor(std::string const & group, std::string const & hex_color) {
    if (_features.find(group) == _features.end() || !_features[group].isGroup) {
        return;
    }

    // Set color for group
    setFeatureColor(group, hex_color);

    // Set same color for all children
    for (auto const & childKey: _features[group].children) {
        setFeatureColor(childKey, hex_color);
    }

    // Emit signal for all affected features
    std::vector<std::string> features = _features[group].children;
    features.push_back(group);
    emit colorChangeFeatures(features, hex_color);
}

void Feature_Tree_Widget::refreshTree() {
    _refreshFeatures();
}

void Feature_Tree_Widget::_itemSelected(QTreeWidgetItem * item, int column) {
    if (!item) return;

    std::string key = item->text(0).toStdString();
    std::vector<std::string> selectedFeatures;

    // Check if it's a group
    if (_features.find(key) != _features.end()) {
        if (_features[key].isGroup) {
            selectedFeatures = _features[key].children;
        } else {
            selectedFeatures = {key};
        }
    }

    emit featuresSelected(selectedFeatures);
}

void Feature_Tree_Widget::_itemChanged(QTreeWidgetItem * item, int column) {
    if (!item || column != 2) return;// Only process checkbox column

    std::string key = item->text(0).toStdString();
    bool enabled = item->checkState(2) == Qt::Checked;

    // Update the feature state
    if (_features.find(key) != _features.end()) {
        _features[key].enabled = enabled;

        std::vector<std::string> affectedFeatures;

        // Handle group toggling
        if (_features[key].isGroup) {
            // Update all children checkboxes
            _updateChildrenState(item, column);

            // Add all children to affected features
            affectedFeatures = _features[key].children;
        } else {
            affectedFeatures = {key};

            // Update parent state if needed
            _updateParentState(item, column);
        }

        // Emit signals
        if (enabled) {
            emit addFeatures(affectedFeatures);
        } else {
            emit removeFeatures(affectedFeatures);
        }
    }
}

void Feature_Tree_Widget::_handleColorChange(QTreeWidgetItem * item, std::string const & color) {
    if (!item) return;

    std::string key = item->text(0).toStdString();

    // Update color in data structure
    if (_features.find(key) != _features.end()) {
        _features[key].color = color;

        std::vector<std::string> affectedFeatures;

        // If it's a group, apply to all children
        if (_features[key].isGroup) {
            for (auto const & childKey: _features[key].children) {
                setFeatureColor(childKey, color);
            }
            affectedFeatures = _features[key].children;
        } else {
            affectedFeatures = {key};
        }

        // Add group itself to affected features
        affectedFeatures.push_back(key);

        // Emit signal
        emit colorChangeFeatures(affectedFeatures, color);
    }
}

void Feature_Tree_Widget::_refreshFeatures() {
    // Clear existing data
    ui->treeWidget->clear();
    _feature_items.clear();
    _group_items.clear();
    _features.clear();

    // Populate tree
    _populateTree();
}

void Feature_Tree_Widget::_populateTree() {
    // Get all keys
    auto allKeys = _data_manager->getAllKeys();

    // First pass: identify groups
    std::unordered_map<std::string, std::vector<std::string>> groups;

    for (auto const & key: allKeys) {
        // Skip if type doesn't match filter
        std::string type = _data_manager->getType(key);
        if (!_type_filters.empty() && !_hasTypeFilter(type)) {
            continue;
        }

        // Try to extract group name
        std::string groupName = _extractGroupName(key);

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
            groupFeature.color = generateRandomColor();
            _features[groupName] = groupFeature;

            // Create tree item
            _addFeatureToTree(groupName, true);

            // Add children to the group
            for (auto const & member: members) {
                // Create feature
                TreeFeature childFeature;
                childFeature.key = member;
                childFeature.type = _data_manager->getType(member);
                childFeature.timeFrame = _data_manager->getTimeFrame(member);
                childFeature.isGroup = false;
                childFeature.color = _features[groupName].color;// Inherit parent color
                _features[member] = childFeature;

                // Add as child to tree
                QTreeWidgetItem * groupItem = _group_items[groupName];
                QTreeWidgetItem * childItem = new QTreeWidgetItem(groupItem);
                childItem->setText(0, QString::fromStdString(member));
                childItem->setText(1, QString::fromStdString(childFeature.type));
                childItem->setFlags(childItem->flags() | Qt::ItemIsUserCheckable);

                _setupCheckboxColumn(childItem, 2, false);
                _setupColorColumn(childItem, 3, childFeature.color);

                _feature_items[member] = childItem;
            }
        }
    }

    // Add standalone items (not in any group)
    for (auto const & key: allKeys) {
        // Skip if type doesn't match filter
        std::string type = _data_manager->getType(key);
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
            feature.type = type;
            feature.timeFrame = _data_manager->getTimeFrame(key);
            feature.isGroup = false;
            feature.color = generateRandomColor();
            _features[key] = feature;

            // Add to tree
            _addFeatureToTree(key);
        }
    }

    // Expand all items for better visibility
    ui->treeWidget->expandAll();
}

std::string Feature_Tree_Widget::_extractGroupName(std::string const & key) {
    std::regex pattern(_grouping_pattern);
    std::smatch matches;

    if (std::regex_search(key, matches, pattern) && matches.size() > 1) {
        return matches[1].str();
    }

    return key;// Return the key itself if no match
}

std::vector<std::string> Feature_Tree_Widget::_getChildFeatures(QTreeWidgetItem * item) {
    std::vector<std::string> children;
    if (!item) return children;

    for (int i = 0; i < item->childCount(); i++) {
        QTreeWidgetItem * child = item->child(i);
        children.push_back(child->text(0).toStdString());
    }

    return children;
}

void Feature_Tree_Widget::_addFeatureToTree(std::string const & key, bool isGroup) {
    if (_features.find(key) == _features.end()) {
        return;
    }

    QTreeWidgetItem * item = new QTreeWidgetItem(ui->treeWidget);
    item->setText(0, QString::fromStdString(key));
    item->setText(1, QString::fromStdString(_features[key].type));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

    _setupCheckboxColumn(item, 2, false);
    _setupColorColumn(item, 3, _features[key].color);

    if (isGroup) {
        _group_items[key] = item;
    } else {
        _feature_items[key] = item;
    }
}

void Feature_Tree_Widget::_setupTreeItem(QTreeWidgetItem * item, TreeFeature const & feature) {
    item->setText(0, QString::fromStdString(feature.key));
    item->setText(1, QString::fromStdString(feature.type));

    _setupCheckboxColumn(item, 2, feature.enabled);
    _setupColorColumn(item, 3, feature.color);
}

void Feature_Tree_Widget::_setupColorColumn(QTreeWidgetItem * item, int column, std::string const & color) {
    ColorWidget * colorWidget = new ColorWidget();
    colorWidget->setText(QString::fromStdString(color));
    ui->treeWidget->setItemWidget(item, column, colorWidget);

    // Connect color change signal
    connect(colorWidget, &ColorWidget::colorChanged, [this, item](QString const & color) {
        _handleColorChange(item, color.toStdString());
    });
}

void Feature_Tree_Widget::_setupCheckboxColumn(QTreeWidgetItem * item, int column, bool checked) {
    item->setCheckState(column, checked ? Qt::Checked : Qt::Unchecked);
}

bool Feature_Tree_Widget::_hasTypeFilter(std::string const & type) {
    return std::find(_type_filters.begin(), _type_filters.end(), type) != _type_filters.end();
}

void Feature_Tree_Widget::_updateChildrenState(QTreeWidgetItem * parent, int column) {
    if (!parent) return;

    Qt::CheckState parentState = parent->checkState(column);

    // Update all children with parent's state
    for (int i = 0; i < parent->childCount(); i++) {
        QTreeWidgetItem * child = parent->child(i);
        child->setCheckState(column, parentState);

        // Update feature state
        std::string childKey = child->text(0).toStdString();
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
    std::string parentKey = parent->text(0).toStdString();
    if (_features.find(parentKey) != _features.end()) {
        _features[parentKey].enabled = (parent->checkState(column) == Qt::Checked);
    }
}
