
#ifndef FEATURE_TREE_WIDGET_HPP
#define FEATURE_TREE_WIDGET_HPP

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
    void setTypeFilters(std::vector<std::string> types);

    // Get selected features (single item or group)
    [[nodiscard]] std::vector<std::string> getSelectedFeatures() const;

    // Get/set feature color
    std::string getFeatureColor(std::string const & key);
    void setFeatureColor(std::string const & key, std::string const & hex_color);

    // Get/set group color (applies to all children)
    std::string getGroupColor(std::string const & group);
    void setGroupColor(std::string const & group, std::string const & hex_color);

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
    void _handleColorChange(QTreeWidgetItem * item, std::string const & color);
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
    std::vector<std::string> _type_filters;

    // Maps feature keys to their tree items
    std::unordered_map<std::string, QTreeWidgetItem *> _feature_items;

    // Maps group names to their tree items
    std::unordered_map<std::string, QTreeWidgetItem *> _group_items;

    // Stores information about features and groups
    std::unordered_map<std::string, TreeFeature> _features;

    // Helper methods
    void _populateTree();
    std::string _extractGroupName(std::string const & key);
    std::vector<std::string> _getChildFeatures(QTreeWidgetItem * item);
    void _addFeatureToTree(std::string const & key, bool isGroup = false);
    void _setupTreeItem(QTreeWidgetItem * item, TreeFeature const & feature);
    void _setupColorColumn(QTreeWidgetItem * item, int column, std::string const & color);
    void _setupCheckboxColumn(QTreeWidgetItem * item, int column, bool checked);
    bool _hasTypeFilter(std::string const & type);
    void _updateChildrenState(QTreeWidgetItem * parent, int column);
    void _updateParentState(QTreeWidgetItem * child, int column);
};

#endif// FEATURE_TREE_WIDGET_HPP
