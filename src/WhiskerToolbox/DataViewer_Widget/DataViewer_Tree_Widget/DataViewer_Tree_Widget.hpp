#ifndef DATAVIEWER_TREE_WIDGET_HPP
#define DATAVIEWER_TREE_WIDGET_HPP

#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QCheckBox>
#include <QLabel>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class DataManager;
enum class DM_DataType;

struct SeriesGroup {
    std::string prefix;
    DM_DataType data_type;
    std::vector<std::string> series_keys;
    QTreeWidgetItem* tree_item = nullptr;
    bool all_enabled = false;
};

class DataViewer_Tree_Widget : public QTreeWidget {
    Q_OBJECT

public:
    explicit DataViewer_Tree_Widget(QWidget* parent = nullptr);
    ~DataViewer_Tree_Widget() override;

    void setDataManager(std::shared_ptr<DataManager> data_manager);
    void populateTree();
    void setTypeFilter(std::vector<DM_DataType> const & types);
    
    // Get a default color for a series (for now, just return a default)
    std::string getSeriesColor(std::string const & series_key) const {
        // For now, return a default blue color
        // Later we can implement proper color management
        return "#0000FF";
    }

signals:
    void seriesToggled(QString const & series_key, bool enabled);
    void groupToggled(QString const & group_prefix, DM_DataType data_type, bool enabled);
    void seriesSelected(QString const & series_key);

private slots:
    void _onItemChanged(QTreeWidgetItem* item, int column);
    void _onItemClicked(QTreeWidgetItem* item, int column);

private:
    std::shared_ptr<DataManager> _data_manager;
    std::vector<DM_DataType> _type_filter;
    std::unordered_map<std::string, std::unique_ptr<SeriesGroup>> _groups;
    bool _updating_items = false; // Flag to prevent recursion
    
    // State preservation for auto-population
    std::unordered_set<std::string> _enabled_series; // Track which series are enabled
    std::unordered_map<std::string, bool> _group_enabled_state; // Track which groups are enabled
    
    void _createGroups();
    void _addSeriesToGroup(std::string const & series_key, DM_DataType data_type);
    std::string _extractPrefix(std::string const & series_key);
    void _updateGroupCheckState(SeriesGroup* group);
    void _setGroupEnabled(SeriesGroup* group, bool enabled);
    
    // Auto-population with state preservation
    void _autoPopulateTree();
    void _saveCurrentState();
    void _restoreState();
    
    // Helper to get data type string for display
    QString _getDataTypeString(DM_DataType type) const;
};

#endif // DATAVIEWER_TREE_WIDGET_HPP 