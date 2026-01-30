#ifndef MASK_INSPECTOR_HPP
#define MASK_INSPECTOR_HPP

/**
 * @file MaskInspector.hpp
 * @brief Inspector widget for MaskData
 * 
 * MaskInspector provides inspection capabilities for MaskData objects.
 * It contains image size management and export functionality.
 * 
 * ## Features
 * - Image size management (set, copy from media)
 * - Export to image and HDF5 formats
 * - Media frame export
 * 
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

#include "nlohmann/json.hpp"

#include <memory>

namespace Ui {
class MaskInspector;
}

class ImageMaskSaver_Widget;
class HDF5MaskSaver_Widget;
class MediaExport_Widget;
class MaskTableView;

// JSON-based saver options - no need for variant types
using MaskSaverConfig = nlohmann::json;

/**
 * @brief Inspector widget for MaskData
 * 
 * Provides mask data inspection and management functionality including
 * image size management and export capabilities.
 */
class MaskInspector : public BaseInspector {
    Q_OBJECT

public:
    /**
     * @brief Construct the mask inspector
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group features
     * @param parent Parent widget
     */
    explicit MaskInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager = nullptr,
        QWidget * parent = nullptr);

    ~MaskInspector() override;

    // =========================================================================
    // IDataInspector Interface
    // =========================================================================

    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Mask; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Mask"); }

    [[nodiscard]] bool supportsExport() const override { return true; }

    /**
     * @brief Set the data view to use for filtering
     * @param view Pointer to the MaskTableView (can be nullptr)
     * 
     * This connects the inspector's group filter to the view panel's table.
     * Should be called when both the inspector and view are created.
     */
    void setDataView(MaskTableView * view);

private slots:
    void _loadSamModel();
    void _onDataChanged();

    // Export slots
    void _onExportTypeChanged(int index);
    void _handleSaveImageMaskRequested(QString format, nlohmann::json config);
    void _onExportMediaFramesCheckboxToggled(bool checked);
    void _onApplyImageSizeClicked();
    void _onCopyImageSizeClicked();
    void _populateMediaComboBox();
    
    // Group filter slots
    void _onGroupFilterChanged(int index);
    void _onGroupChanged();
    
    // Move/copy/delete slots
    void _onMoveMasksRequested(std::string const & target_key);
    void _onCopyMasksRequested(std::string const & target_key);
    void _onMoveMasksToGroupRequested(int group_id);
    void _onRemoveMasksFromGroupRequested();
    void _onDeleteMasksRequested();

private:
    void _connectSignals();
    void _updateImageSizeDisplay();
    void _initiateSaveProcess(QString const & format, MaskSaverConfig const & config);
    bool _performRegistrySave(QString const & format, MaskSaverConfig const & config);
    void _populateGroupFilterCombo();

    Ui::MaskInspector * ui;
    int _dm_observer_id{-1};  ///< Callback ID for DataManager-level observer
    MaskTableView * _data_view{nullptr};  ///< Pointer to the associated table view (optional)
};

#endif // MASK_INSPECTOR_HPP
