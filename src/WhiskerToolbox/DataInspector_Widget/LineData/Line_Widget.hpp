#ifndef LINE_WIDGET_HPP
#define LINE_WIDGET_HPP

#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "Entity/EntityTypes.hpp"

#include "nlohmann/json.hpp"
#include <QString>
#include <QWidget>

#include <memory>
#include <string>

namespace Ui {
class Line_Widget;
}
class DataManager;
class LineTableModel;
class CSVLineSaver_Widget;
class BinaryLineSaver_Widget;
class QStackedWidget;
class QComboBox;
class QModelIndex;
class QCheckBox;
class GroupManager;

// JSON-based saver configuration - no need for variant types
using LineSaverConfig = nlohmann::json;

class Line_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Line_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Line_Widget() override;

    void openWidget();
    void setActiveKey(std::string const & key);
    void removeCallbacks();
    void updateTable();
    void setGroupManager(GroupManager * group_manager);

signals:
    void frameSelected(int frame_id);

private:
    Ui::Line_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    LineTableModel * _line_table_model;
    std::string _active_key;
    int _callback_id{-1};
    int _dm_observer_id{-1};  ///< Callback ID for DataManager-level observer
    GroupManager * _group_manager{nullptr};

    // No longer need SaverType enum - use format strings directly

    /**
     * @brief Move selected line to the specified target key
     * 
     * @param target_key The key to move the line to
     */
    void _moveLineToTarget(std::string const & target_key);

    /**
     * @brief Copy selected line to the specified target key
     * 
     * @param target_key The key to copy the line to
     */
    void _copyLineToTarget(std::string const & target_key);

    /**
     * @brief Show context menu for the table view
     * 
     * @param position The position where the context menu should appear
     */
    void _showContextMenu(QPoint const & position);

private slots:
    void _handleCellDoubleClicked(QModelIndex const & index);
    void _onDataChanged();
    void _deleteSelectedLine();

    void _onExportTypeChanged(int index);
    void _handleSaveCSVRequested(QString const & format, nlohmann::json const & config);
    void _handleSaveMultiFileCSVRequested(QString const & format, nlohmann::json const & config);
    void _handleSaveBinaryRequested(QString const & format, nlohmann::json const & config);
    void _onExportMediaFramesCheckboxToggled(bool checked);
    void _onApplyImageSizeClicked();
    void _onCopyImageSizeClicked();
    void _onGroupFilterChanged(int index);
    void _onGroupChanged();
    void _onAutoScrollToCurrentFrame();

private:
    void _initiateSaveProcess(QString const & format, LineSaverConfig const & config);
    bool _performRegistrySave(QString const & format, LineSaverConfig const & config);
    void _updateImageSizeDisplay();
    void _populateMediaComboBox();
    void _populateGroupFilterCombo();
    void _populateGroupSubmenu(QMenu * menu, bool for_moving);
    void _moveSelectedLinesToGroup(int group_id);
    void _removeSelectedLinesFromGroup();

    /**
     * @brief Get selected frames from the table view
     * 
     * @return Vector of unique frame numbers that are currently selected
     */
    std::vector<TimeFrameIndex> _getSelectedFrames();

    /**
     * @brief Get selected EntityIds from the table view
     * 
     * @return Vector of EntityIds that are currently selected
     */
    std::vector<EntityId> _getSelectedEntityIds();
};

#endif// LINE_WIDGET_HPP
