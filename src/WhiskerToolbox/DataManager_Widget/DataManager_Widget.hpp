#ifndef DATAMANAGER_WIDGET_HPP
#define DATAMANAGER_WIDGET_HPP

#include <QString>
#include <QScrollArea>
#include <QShowEvent>

#include <memory>
#include <string>


namespace Ui {
class DataManager_Widget;
}

class DataManager;
class DataManagerWidgetState;
class SelectionContext;
class TimeScrollBar;
class GroupManager;
class QResizeEvent;
class WorkspaceManager;

class DataManager_Widget : public QScrollArea {
    Q_OBJECT
public:
    DataManager_Widget(std::shared_ptr<DataManager> data_manager,
                       TimeScrollBar * time_scrollbar,
                       WorkspaceManager * workspace_manager = nullptr,
                       QWidget * parent = nullptr);
    ~DataManager_Widget() override;

    void openWidget();// Call

    /**
     * @brief Set the GroupManager for group functionality
     * @param group_manager The GroupManager instance
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Clear the currently selected feature and return to no-selection state
     * 
     * This method resets the DataManager_Widget to show no feature selection,
     * clearing any active widgets and resetting the feature label.
     */
    void clearFeatureSelection();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    Ui::DataManager_Widget * ui;
    TimeScrollBar * _time_scrollbar;
    std::shared_ptr<DataManager> _data_manager;
    WorkspaceManager * _workspace_manager{nullptr};
    QString _highlighted_available_feature;
    std::vector<int> _current_data_callbacks;
    GroupManager * _group_manager{nullptr};
    
    // Store references to widgets for group manager setup
    class Line_Widget * _line_widget{nullptr};

    // Editor state for workspace serialization and inter-widget communication
    std::shared_ptr<DataManagerWidgetState> _state;
    SelectionContext * _selection_context{nullptr};


private slots:
    /**
     * @brief Handle change in output directory setting
     * 
     * @param dir_name The new directory path as a QString
     */
    void _changeOutputDir(QString dir_name);

    /**
     * @brief Handle selection of a feature from the feature table
     * 
     * This method updates the UI to show the appropriate widget for the selected
     * feature type and updates the feature label to display the selected feature name.
     * 
     * @param feature The name/key of the selected feature
     */
    void _handleFeatureSelected(QString const & feature);

    /**
     * @brief Disable and cleanup the previously selected feature
     * 
     * This method disconnects callbacks and signals for the specified feature
     * and performs necessary cleanup when switching between features.
     * 
     * @param feature The name/key of the feature to disable
     */
    void _disablePreviousFeature(QString const & feature);

    /**
     * @brief Create new data of the specified type with the given key
     * 
     * @param key The unique identifier for the new data
     * @param type The type of data to create (e.g., "Point", "Mask", "Line")
     * @param timeframe_key The timeframe to assign to the new data
     */
    void _createNewData(std::string key, std::string type, std::string timeframe_key);

    /**
     * @brief Handle frame selection from child widgets
     * 
     * @param frame_id The ID of the selected frame
     */
    void _changeScrollbar(int frame_id);

    /**
     * @brief Handle deletion of data from the feature table
     * 
     * This method deletes the specified data from the DataManager and handles
     * cleanup of any currently selected features that might be affected.
     * 
     * @param feature The name/key of the feature to delete
     */
    void _deleteData(QString const & feature);

    /**
     * @brief Show context menu at the specified position
     * 
     * @param pos Global position where the context menu should appear
     */
    void _showContextMenu(QPoint const & pos);
};


#endif// DATAMANAGER_WIDGET_HPP
