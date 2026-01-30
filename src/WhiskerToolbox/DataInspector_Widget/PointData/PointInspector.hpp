#ifndef POINT_INSPECTOR_HPP
#define POINT_INSPECTOR_HPP

/**
 * @file PointInspector.hpp
 * @brief Inspector widget for PointData
 * 
 * PointInspector provides inspection capabilities for PointData objects.
 * It provides functionality for managing point data properties, exporting,
 * and image size configuration.
 * 
 * ## Features
 * - Image size configuration
 * - Group filtering (connects to PointTableView)
 * - Export to CSV
 * - Media frame export
 * 
 * @see BaseInspector for the base class
 * @see PointTableView for the table view component
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"
#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "Entity/EntityTypes.hpp"

#include <memory>
#include <string>
#include <variant>

namespace Ui {
class PointInspector;
}

class CSVPointSaver_Widget;
class MediaData;
class PointTableView;

// Define the variant type for saver options
using PointSaverOptionsVariant = std::variant<CSVPointSaverOptions>;

/**
 * @brief Inspector widget for PointData
 * 
 * Provides properties and controls for PointData inspection, including
 * image size configuration, group filtering, and data export.
 */
class PointInspector : public BaseInspector {
    Q_OBJECT

public:
    /**
     * @brief Construct the point inspector
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group features
     * @param parent Parent widget
     */
    explicit PointInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager = nullptr,
        QWidget * parent = nullptr);

    ~PointInspector() override;

    // =========================================================================
    // IDataInspector Interface
    // =========================================================================

    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Points; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Point"); }

    [[nodiscard]] bool supportsExport() const override { return true; }

    /**
     * @brief Set the PointTableView for group filter coordination
     * @param table_view Pointer to the PointTableView (can be nullptr)
     * 
     * When set, the group filter combo box will control the table view's filtering.
     */
    void setTableView(PointTableView * table_view);

    void setGroupManager(GroupManager * group_manager);

private:
    void _setupUi();
    void _connectSignals();

    enum SaverType { CSV };

    void _saveToCSVFile(CSVPointSaverOptions & options);
    bool _performActualCSVSave(CSVPointSaverOptions & options);
    void _initiateSaveProcess(SaverType saver_type, PointSaverOptionsVariant & options_variant);

    void _updateImageSizeDisplay();
    void _populateMediaComboBox();
    void _populateGroupFilterCombo();

    Ui::PointInspector * ui;
    PointTableView * _table_view{nullptr};
    int _previous_frame{0};
    int _dm_observer_id{-1};  ///< Callback ID for DataManager-level observer

private slots:
    void _onExportTypeChanged(int index);
    void _handleSaveCSVRequested(CSVPointSaverOptions options);
    void _onExportMediaFramesCheckboxToggled(bool checked);
    void _onApplyImageSizeClicked();
    void _onCopyImageSizeClicked();
    void _onGroupFilterChanged(int index);
    void _onGroupChanged();
    void _onMovePointsRequested(std::string const & target_key);
    void _onCopyPointsRequested(std::string const & target_key);
    void _onMovePointsToGroupRequested(int group_id);
    void _onRemovePointsFromGroupRequested();
    void _onDeletePointsRequested();
};

#endif // POINT_INSPECTOR_HPP
