#ifndef MASK_WIDGET_HPP
#define MASK_WIDGET_HPP

#include "DataManager/Masks/IO/Image/Mask_Data_Image.hpp"
#include "DataManager_Widget/utils/DataManager_Widget_utils.hpp"// For context menu utilities
#include "DataManager/TimeFrame.hpp"

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

using MaskSaverOptionsVariant = std::variant<ImageMaskSaverOptions>;

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

signals:
    void frameSelected(int frame_id);

private:
    Ui::Mask_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<dl::EfficientSAM> _sam_model;
    std::string _active_key;
    MaskTableModel * _mask_table_model;
    int _callback_id{-1};

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
    void _initiateSaveProcess(SaverType saver_type, MaskSaverOptionsVariant & options_variant);
    bool _performActualImageSave(ImageMaskSaverOptions & options);

private slots:
    void _loadSamModel();
    void _handleTableViewDoubleClicked(QModelIndex const & index);
    void _onDataChanged();
    void _deleteSelectedMasks();// New slot for delete operation

    // Export slots
    void _onExportTypeChanged(int index);
    void _handleSaveImageMaskRequested(ImageMaskSaverOptions options);
    void _onExportMediaFramesCheckboxToggled(bool checked);
};


#endif// MASK_WIDGET_HPP
