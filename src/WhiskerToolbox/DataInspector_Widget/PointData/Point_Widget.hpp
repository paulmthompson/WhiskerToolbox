#ifndef POINT_WIDGET_HPP
#define POINT_WIDGET_HPP

#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"         // For CSVPointSaverOptions
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"// For context menu utilities
#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityTypes.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <variant>// std::variant

namespace Ui {
class Point_Widget;
}

class DataManager;
class PointTableModel;
class CSVPointSaver_Widget;
class MediaData;
class GroupManager;

// Define the variant type for saver options
using PointSaverOptionsVariant = std::variant<CSVPointSaverOptions>;

/**
 * @brief Point Widget Class
 *
 * This widget is used for visualizing and managing point data in a table view.
 * It provides functionality to save point data to various formats and export
 * matching media frames.
 *
 * Point data is organized by time (frame), with each frame potentially containing
 * multiple points. The widget allows for moving and copying points between different
 * PointData instances via right-click context menu.
 */
class Point_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Point_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Point_Widget() override;

    void openWidget();
    void setActiveKey(std::string const & key);

    void updateTable();

    void removeCallbacks();
    void setGroupManager(GroupManager * group_manager);

signals:
    void frameSelected(int frame_id);

private:
    Ui::Point_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    PointTableModel * _point_table_model;
    std::string _active_key;
    int _previous_frame{0};
    int _callback_id{-1};
    int _dm_observer_id{-1};  ///< Callback ID for DataManager-level observer
    GroupManager * _group_manager{nullptr};

    enum SaverType { CSV };

    void _saveToCSVFile(CSVPointSaverOptions & options);
    bool _performActualCSVSave(CSVPointSaverOptions & options);
    void _initiateSaveProcess(SaverType saver_type, PointSaverOptionsVariant & options_variant);

    /**
     * @brief Get frame numbers from selected table rows
     * 
     * @return Vector of frame numbers corresponding to selected rows
     */
    std::vector<TimeFrameIndex> _getSelectedFrames();

    /**
     * @brief Get selected EntityIds from the table view
     * 
     * @return Vector of EntityIds that are currently selected
     */
    std::vector<EntityId> _getSelectedEntityIds();

    /**
     * @brief Move selected points to the specified target key
     * 
     * @param target_key The key to move points to
     */
    void _movePointsToTarget(std::string const & target_key);

    /**
     * @brief Copy selected points to the specified target key
     * 
     * @param target_key The key to copy points to
     */
    void _copyPointsToTarget(std::string const & target_key);

    /**
     * @brief Show context menu for the table view
     * 
     * @param position The position where the context menu should appear
     */
    void _showContextMenu(QPoint const & position);

private slots:
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();
    void _onExportTypeChanged(int index);
    void _handleSaveCSVRequested(CSVPointSaverOptions options);
    void _onExportMediaFramesCheckboxToggled(bool checked);
    void _deleteSelectedPoints();
    void _onApplyImageSizeClicked();
    void _onCopyImageSizeClicked();
    void _updateImageSizeDisplay();
    void _populateMediaComboBox();
    void _onGroupFilterChanged(int index);
    void _onGroupChanged();
    void _populateGroupFilterCombo();
    void _populateGroupSubmenu(QMenu * menu, bool for_moving);
    void _moveSelectedPointsToGroup(int group_id);
    void _removeSelectedPointsFromGroup();
};

#endif// POINT_WIDGET_HPP
