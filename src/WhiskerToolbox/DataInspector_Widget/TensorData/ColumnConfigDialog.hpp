#ifndef COLUMN_CONFIG_DIALOG_HPP
#define COLUMN_CONFIG_DIALOG_HPP

/**
 * @file ColumnConfigDialog.hpp
 * @brief Dialog for configuring a single tensor column
 *
 * ColumnConfigDialog allows the user to:
 * 1. Select a DataManager source key
 * 2. Configure a TransformPipeline via JSON or the TransformsV2 widget
 * 3. Optionally mark the column as an interval property (start/end/duration)
 * 4. Set the column name
 *
 * Every column is fully described by a (source_key, pipeline_json) pair.
 * The "Request from Transforms V2" button is the primary way to build pipelines.
 *
 * @see TensorDesigner for the hosting panel
 * @see TensorColumnBuilders::ColumnRecipe for the output type
 */

#include <QDialog>

#include <memory>
#include <string>
#include <vector>

class DataManager;
class QComboBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QVBoxLayout;
class QWidget;

enum class DesignerRowType;

namespace EditorLib {
class OperationContext;
struct PendingOperation;
struct OperationResult;
struct EditorInstanceId;
struct OperationId;
}// namespace EditorLib

namespace WhiskerToolbox::TensorBuilders {
struct ColumnRecipe;
enum class IntervalProperty : std::uint8_t;
}// namespace WhiskerToolbox::TensorBuilders

/**
 * @brief Dialog for configuring a tensor column
 *
 * Shows source selection, pipeline JSON editing, and optional interval
 * property configuration. Returns a ColumnRecipe on accept.
 */
class ColumnConfigDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Construct column configuration dialog
     * @param data_manager DataManager for source key enumeration
     * @param row_type Current row type of the tensor designer
     * @param operation_context OperationContext for requesting pipelines from TransformsV2 (nullable)
     * @param parent Parent widget
     */
    explicit ColumnConfigDialog(
            std::shared_ptr<DataManager> data_manager,
            DesignerRowType row_type,
            EditorLib::OperationContext * operation_context = nullptr,
            QWidget * parent = nullptr);

    /**
     * @brief Construct dialog pre-populated with an existing recipe
     * @param data_manager DataManager for source key enumeration
     * @param row_type Current row type
     * @param recipe Existing recipe to edit
     * @param operation_context OperationContext for requesting pipelines from TransformsV2 (nullable)
     * @param parent Parent widget
     */
    ColumnConfigDialog(
            std::shared_ptr<DataManager> data_manager,
            DesignerRowType row_type,
            WhiskerToolbox::TensorBuilders::ColumnRecipe const & recipe,
            EditorLib::OperationContext * operation_context = nullptr,
            QWidget * parent = nullptr);

    ~ColumnConfigDialog() override;

    /**
     * @brief Get the configured column recipe
     * @return The ColumnRecipe built from user selections
     */
    [[nodiscard]] WhiskerToolbox::TensorBuilders::ColumnRecipe getRecipe() const;

private slots:
    void _onSourceKeyChanged(int index);
    void _onColumnNameEdited(QString const & text);
    void _updateAutoName();
    void _onValidateClicked();
    void _onRequestTV2Clicked();
    void _onOperationDelivered(EditorLib::PendingOperation const & op,
                               EditorLib::OperationResult const & result);
    void _onOperationClosed(EditorLib::OperationId const & id);
    void _onIntervalPropertyToggled(bool checked);

private:
    void _setupUi();
    void _connectSignals();
    void _populateSourceKeys();
    void _applyRecipe(WhiskerToolbox::TensorBuilders::ColumnRecipe const & recipe);

    /// Clean up any pending OperationContext request on dialog close
    void _cleanupPendingOperation();

    /// Reset the "Request from Transforms V2" button to its default state
    void _resetRequestButton();

    std::shared_ptr<DataManager> _data_manager;
    DesignerRowType _row_type;
    EditorLib::OperationContext * _operation_context{nullptr};
    QString _requester_id;        ///< EditorInstanceId for OperationContext requests
    QString _pending_operation_id;///< OperationId of our pending request (empty if none)
    bool _auto_name{true};        ///< Auto-generate column name from source key

    // UI
    QVBoxLayout * _layout{nullptr};

    // Source
    QComboBox * _source_combo{nullptr};
    QLabel * _source_type_label{nullptr};

    // Pipeline JSON (primary column configuration)
    QTextEdit * _pipeline_json_edit{nullptr};
    QPushButton * _validate_btn{nullptr};
    QPushButton * _request_tv2_btn{nullptr};
    QLabel * _validation_label{nullptr};

    // Interval property (visible only for interval row type)
    QGroupBox * _interval_property_group{nullptr};
    QComboBox * _interval_property_combo{nullptr};

    // Column name
    QLineEdit * _name_edit{nullptr};
};

#endif// COLUMN_CONFIG_DIALOG_HPP
