#ifndef TENSOR_DESIGNER_HPP
#define TENSOR_DESIGNER_HPP

/**
 * @file TensorDesigner.hpp
 * @brief Design mode panel for building TensorData with lazy columns
 *
 * TensorDesigner provides an interactive column-building experience
 * for constructing TensorData backed by LazyColumnTensorStorage.
 * Users select a row source (intervals, events, timeframe) and
 * add columns by configuring DataManager source keys with 
 * TransformsV2 range reductions or element transforms.
 *
 * ## Architecture
 * - Row configuration: Select a DigitalIntervalSeries (interval rows),
 *   DigitalEventSeries (timestamp rows), or ordinal rows
 * - Column building: Each column is a ColumnRecipe that maps to a
 *   ColumnProviderFn closure via TensorColumnBuilders
 * - Live preview: Columns are lazily materialized in the TensorDataView
 * - JSON serialization: Column configurations save/load as JSON
 * - CSV export: Export the designed tensor to CSV
 *
 * @see TensorColumnBuilders for the builder layer
 * @see LazyColumnTensorStorage for the storage backend
 * @see TensorInspector for the inspector that hosts this panel
 */

#include <QPointer>
#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class ColumnConfigDialog;
class DataManager;
class DataInspectorState;
class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class TensorData;
class SelectionContext;
struct SelectionSource;

namespace EditorLib {
class OperationContext;
}// namespace EditorLib

namespace WhiskerToolbox::TensorBuilders {
struct ColumnRecipe;
enum class IntervalProperty : std::uint8_t;
}// namespace WhiskerToolbox::TensorBuilders

/**
 * @brief Row source type for the tensor designer
 */
enum class DesignerRowType {
    None,            ///< No row source selected
    Interval,        ///< Rows from DigitalIntervalSeries
    Timestamp,       ///< Rows from DigitalEventSeries
    Ordinal,         ///< Manual ordinal rows
    DerivedFromSource///< Derive row timestamps from any data source's timestamps
};

/**
 * @brief Design mode panel for building TensorData with lazy columns
 *
 * Embedded in TensorInspector when the user wants to create/edit
 * a tensor backed by LazyColumnTensorStorage.
 */
class TensorDesigner : public QWidget {
    Q_OBJECT

public:
    explicit TensorDesigner(
            std::shared_ptr<DataManager> data_manager,
            QWidget * parent = nullptr);
    ~TensorDesigner() override;

    // =========================================================================
    // Selection Context
    // =========================================================================

    /**
     * @brief Set the SelectionContext for dataFocus awareness
     * @param context SelectionContext instance (can be nullptr)
     */
    void setSelectionContext(SelectionContext * context);

    /**
     * @brief Set the OperationContext for inter-widget pipeline requests
     * @param context OperationContext instance (can be nullptr)
     */
    void setOperationContext(EditorLib::OperationContext * context);

    /**
     * @brief Set the DataInspectorState for auto-pinning during dialog interaction
     * @param state Shared state pointer (can be nullptr)
     *
     * When set, the designer will auto-pin the inspector when opening the
     * ColumnConfigDialog to prevent SelectionContext changes from navigating
     * away from the tensor being designed. The pin is restored when the
     * dialog closes.
     */
    void setInspectorState(std::shared_ptr<DataInspectorState> state);

    // =========================================================================
    // Tensor Management
    // =========================================================================

    /**
     * @brief Get the key of the tensor created by this designer
     * @return Data key in DataManager, or empty if no tensor created yet
     */
    [[nodiscard]] std::string tensorKey() const { return _tensor_key; }

    /**
     * @brief Set an existing tensor key to edit (if it has LazyColumnTensorStorage)
     */
    void setTensorKey(std::string const & key);

    // =========================================================================
    // JSON Serialization
    // =========================================================================

    /**
     * @brief Serialize the current design configuration to JSON
     * @return JSON string describing row source + column recipes
     */
    [[nodiscard]] std::string toJson() const;

    /**
     * @brief Load a design configuration from JSON
     * @param json JSON string
     * @return true if successfully loaded
     */
    bool fromJson(std::string const & json);

signals:
    /**
     * @brief Emitted when the tensor has been created or updated
     * @param key DataManager key of the tensor
     */
    void tensorCreated(QString const & key);

    /**
     * @brief Emitted when the designer wants to inspect a data key
     * @param key The data key to focus
     */
    void dataFocusRequested(QString const & key);

private slots:
    void _onRowSourceTypeChanged(int index);
    void _onRowSourceKeyChanged(int index);
    void _onAddColumnClicked();
    void _onRemoveColumnClicked();
    void _onEditColumnClicked();
    void _onColumnDoubleClicked(QListWidgetItem * item);
    void _onBuildClicked();
    void _onSaveJsonClicked();
    void _onLoadJsonClicked();
    void _onDataFocusChanged(
            QString const & data_key,
            QString const & data_type);

private:
    void _setupUi();
    void _connectSignals();
    void _populateRowSourceKeys();
    void _refreshColumnList();
    void _buildTensor();
    void _updateStatus(QString const & message);

    /// Auto-pin the inspector state when opening a dialog
    void _pinInspectorForDialog();

    /// Restore previous pin state when the dialog closes
    void _unpinInspectorAfterDialog();

    /// Handle dialog result for adding a column
    void _onDialogAcceptedAdd();

    /// Handle dialog result for editing a column
    void _onDialogAcceptedEdit(int row);

    // --- Data ---
    std::shared_ptr<DataManager> _data_manager;
    SelectionContext * _selection_context{nullptr};
    EditorLib::OperationContext * _operation_context{nullptr};
    std::shared_ptr<DataInspectorState> _inspector_state;
    std::string _tensor_key;

    // Dialog tracking
    QPointer<ColumnConfigDialog> _active_dialog;///< Active modeless dialog (if any)
    bool _was_pinned_before_dialog{false};      ///< Pin state before dialog opened

    // --- Row configuration ---
    DesignerRowType _row_type{DesignerRowType::None};
    std::string _row_source_key;

    // --- Column recipes ---
    std::vector<WhiskerToolbox::TensorBuilders::ColumnRecipe> _column_recipes;

    // --- UI widgets ---
    QVBoxLayout * _main_layout{nullptr};

    // Row source section
    QLabel * _row_section_label{nullptr};
    QComboBox * _row_type_combo{nullptr};
    QComboBox * _row_source_combo{nullptr};
    QLabel * _row_info_label{nullptr};

    // Column management section
    QLabel * _col_section_label{nullptr};
    QListWidget * _column_list{nullptr};
    QHBoxLayout * _col_button_layout{nullptr};
    QPushButton * _add_col_btn{nullptr};
    QPushButton * _edit_col_btn{nullptr};
    QPushButton * _remove_col_btn{nullptr};

    // Build / Config section
    QHBoxLayout * _action_layout{nullptr};
    QPushButton * _build_btn{nullptr};
    QPushButton * _save_json_btn{nullptr};
    QPushButton * _load_json_btn{nullptr};

    // Status
    QLabel * _status_label{nullptr};
};

#endif// TENSOR_DESIGNER_HPP
