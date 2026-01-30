#ifndef LINE_INSPECTOR_HPP
#define LINE_INSPECTOR_HPP

/**
 * @file LineInspector.hpp
 * @brief Inspector widget for LineData
 * 
 * LineInspector provides inspection capabilities for LineData objects.
 * It integrates LineTableView for data display and provides additional
 * functionality for image size management, export, and group operations.
 * 
 * ## Features
 * - Line data table with frame and polyline information (via LineTableView)
 * - Group filtering
 * - Context menu for move/copy/delete operations
 * - Export to CSV and binary formats
 * - Image size management
 * - Frame navigation via double-click
 * 
 * @see LineTableView for the table view implementation
 * @see BaseInspector for the base class
 * 
 * @note This inspector replaces the legacy Line_Widget, consolidating
 * functionality into the modern inspector architecture.
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

namespace Ui {
class LineInspector;
}

class CSVLineSaver_Widget;
class BinaryLineSaver_Widget;
class LineTableView;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QLabel;

// JSON-based saver configuration
using LineSaverConfig = nlohmann::json;

/**
 * @brief Inspector widget for LineData
 * 
 * Wraps Line_Widget to provide IDataInspector interface while reusing
 * existing functionality for line data inspection and management.
 */
class LineInspector : public BaseInspector {
    Q_OBJECT

public:
    /**
     * @brief Construct the line inspector
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group features
     * @param parent Parent widget
     */
    explicit LineInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager = nullptr,
        QWidget * parent = nullptr);

    ~LineInspector() override;

    // =========================================================================
    // IDataInspector Interface
    // =========================================================================

    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Line; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Line"); }

    [[nodiscard]] bool supportsExport() const override { return true; }

    /**
     * @brief Set the data view to use for filtering
     * @param view Pointer to the LineTableView (can be nullptr)
     * 
     * This connects the inspector's group filter to the view panel's table.
     * Should be called when both the inspector and view are created.
     */
    void setDataView(LineTableView * view);

private slots:
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
    void _onMoveLinesRequested(std::string const & target_key);
    void _onCopyLinesRequested(std::string const & target_key);
    void _onMoveLinesToGroupRequested(int group_id);
    void _onRemoveLinesFromGroupRequested();
    void _onDeleteLinesRequested();

private:
    void _setupUi();
    void _connectSignals();
    void _initiateSaveProcess(QString const & format, LineSaverConfig const & config);
    bool _performRegistrySave(QString const & format, LineSaverConfig const & config);
    void _updateImageSizeDisplay();
    void _populateMediaComboBox();
    void _populateGroupFilterCombo();

    Ui::LineInspector * _ui{nullptr};
    int _dm_observer_id{-1};  ///< Callback ID for DataManager-level observer
    LineTableView * _data_view{nullptr};  ///< Pointer to the associated table view
};

#endif // LINE_INSPECTOR_HPP
