#ifndef MASK_WIDGET_HPP
#define MASK_WIDGET_HPP

// Remove direct OpenCV dependency - use registry system instead
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"// For context menu utilities
#include "TimeFrame/TimeFrame.hpp"
#include "nlohmann/json.hpp"

#include <QModelIndex>
#include <QWidget>

#include <memory>
#include <string>
#include <variant>

namespace dl {
class EfficientSAM;
};

namespace Ui {
class Mask_Widget;
}

class DataManager;
class MaskTableModel;
class ImageMaskSaver_Widget;
class HDF5MaskSaver_Widget;
class MediaExport_Widget;
class GroupManager;

// JSON-based saver options - no need for variant types
using MaskSaverConfig = nlohmann::json;

class Mask_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Mask_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~Mask_Widget() override;

    void openWidget();// Call
    void selectPoint(float x, float y);
    void setActiveKey(std::string const & key);
    void removeCallbacks();
    void updateTable();
    void setGroupManager(GroupManager * group_manager);

signals:
    void frameSelected(int frame_id);

private:
    Ui::Mask_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<dl::EfficientSAM> _sam_model;
    std::string _active_key;
    std::unique_ptr<MaskTableModel> _mask_table_model;
    int _callback_id{-1};
    GroupManager * _group_manager{nullptr};

    /**
     * @brief Get frame numbers from selected table rows
     * 
     * @return Vector of frame numbers corresponding to selected rows
     */
    std::vector<TimeFrameIndex> _getSelectedFrames();

    /**
     * @brief Move selected masks to the specified target key
     * 
     * @param target_key The key to move masks to
     */
    void _moveMasksToTarget(std::string const & target_key);

    /**
     * @brief Copy selected masks to the specified target key
     * 
     * @param target_key The key to copy masks to
     */
    void _copyMasksToTarget(std::string const & target_key);

    /**
     * @brief Show context menu for the table view
     * 
     * @param position The position where the context menu should appear
     */
    void _showContextMenu(QPoint const & position);

    /**
     * @brief Update the ImageSize display labels
     * 
     * Updates the UI labels to show the current ImageSize of the active mask data
     */
    void _updateImageSizeDisplay();

    enum SaverType { HDF5,
                     IMAGE };
    void _initiateSaveProcess(QString const& format, MaskSaverConfig const& config);
    bool _performRegistrySave(QString const& format, MaskSaverConfig const& config);

private slots:
    void _loadSamModel();
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();
    void _deleteSelectedMasks();// New slot for delete operation

    // Export slots
    void _onExportTypeChanged(int index);
    void _handleSaveImageMaskRequested(QString format, nlohmann::json config);
    void _onExportMediaFramesCheckboxToggled(bool checked);
    void _onApplyImageSizeClicked();
    void _onCopyImageSizeClicked();
    void _populateMediaComboBox();
    void _onGroupFilterChanged(int index);
    void _onGroupChanged();
    void _populateGroupFilterCombo();
    void _populateGroupSubmenu(QMenu * menu, bool for_moving);
    void _moveSelectedMasksToGroup(int group_id);
    void _removeSelectedMasksFromGroup();
};


#endif// MASK_WIDGET_HPP
