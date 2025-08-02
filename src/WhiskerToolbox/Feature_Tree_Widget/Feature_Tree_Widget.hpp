
#ifndef FEATURE_TREE_WIDGET_HPP
#define FEATURE_TREE_WIDGET_HPP

#include "DataManagerTypes.hpp"

#include <QTreeWidget>
#include <QWidget>

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

class DataManager;
class QPushButton;
class QTreeWidgetItem;

namespace Ui {
class Feature_Tree_Widget;
}

class Feature_Tree_Widget : public QWidget {
    Q_OBJECT

public:
    explicit Feature_Tree_Widget(QWidget * parent = nullptr);
    ~Feature_Tree_Widget() override;

    void setDataManager(std::shared_ptr<DataManager> data_manager);

    // Set grouping pattern (e.g. "(.+)_\d+$" for name_number pattern)
    void setGroupingPattern(std::string pattern);

    // Set types to filter
    void setTypeFilters(std::vector<DM_DataType> types);

    // Get selected features (single item or group)
    [[nodiscard]] std::vector<std::string> getSelectedFeatures() const;

    // Refresh the tree view
    void refreshTree();

signals:
    void featuresSelected(std::vector<std::string> const & features);
    void addFeatures(std::vector<std::string> const & features);
    void removeFeatures(std::vector<std::string> const & features);
    void colorChangeFeatures(std::vector<std::string> const & features, std::string const & hex_color);

private slots:
    void _itemSelected(QTreeWidgetItem * item, int column);
    void _itemChanged(QTreeWidgetItem * item, int column);
    void _refreshFeatures();

private:
    struct TreeFeature {
        std::string key;
        std::string type;
        std::string timeFrame;
        bool isGroup;
        std::string color;
        bool enabled = false;
        std::vector<std::string> children;
    };

    Ui::Feature_Tree_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::string _grouping_pattern = "(.+)_\\d+$";// Default pattern: name_number
    std::vector<DM_DataType> _type_filters;

    // Maps feature keys to their tree items
    std::unordered_map<std::string, QTreeWidgetItem *> _feature_items;

    // Maps group names to their tree items
    std::unordered_map<std::string, QTreeWidgetItem *> _group_items;

    // Stores information about features and groups
    std::unordered_map<std::string, TreeFeature> _features;

    // Helper methods
    void _populateTree();
    std::string _extractGroupName(std::string const & key);
    void _addFeatureToTree(std::string const & key, bool isGroup = false);
    void _setupTreeItem(QTreeWidgetItem * item, TreeFeature const & feature);
    bool _hasTypeFilter(DM_DataType const & type);
    void _updateChildrenState(QTreeWidgetItem * parent, int column);
    void _updateParentState(QTreeWidgetItem * child, int column);
};

void setup_checkbox_column(QTreeWidgetItem * item, int column, bool checked);

std::vector<std::string> get_child_features(QTreeWidgetItem * item);

#endif// FEATURE_TREE_WIDGET_HPP
