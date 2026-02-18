#ifndef COLUMN_CONFIG_DIALOG_HPP
#define COLUMN_CONFIG_DIALOG_HPP

/**
 * @file ColumnConfigDialog.hpp
 * @brief Dialog for configuring a single tensor column
 *
 * ColumnConfigDialog allows the user to:
 * 1. Select a DataManager source key
 * 2. Choose an operation (range reduction, passthrough, interval property, etc.)
 * 3. Configure transform parameters (e.g., offset for AnalogSampleAtOffset)
 * 4. Set the column name
 *
 * The dialog produces a ColumnRecipe that can be used with TensorColumnBuilders
 * to create a ColumnProviderFn closure.
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
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QTextEdit;
class QVBoxLayout;
class QWidget;

enum class DesignerRowType;

namespace WhiskerToolbox::TensorBuilders {
struct ColumnRecipe;
enum class IntervalProperty : std::uint8_t;
} // namespace WhiskerToolbox::TensorBuilders

/**
 * @brief Dialog for configuring a tensor column
 *
 * Shows source selection, operation selection, and parameter editing.
 * Returns a ColumnRecipe on accept.
 */
class ColumnConfigDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Construct column configuration dialog
     * @param data_manager DataManager for source key enumeration
     * @param row_type Current row type of the tensor designer
     * @param parent Parent widget
     */
    explicit ColumnConfigDialog(
        std::shared_ptr<DataManager> data_manager,
        DesignerRowType row_type,
        QWidget * parent = nullptr);

    /**
     * @brief Construct dialog pre-populated with an existing recipe
     * @param data_manager DataManager for source key enumeration
     * @param row_type Current row type
     * @param recipe Existing recipe to edit
     * @param parent Parent widget
     */
    ColumnConfigDialog(
        std::shared_ptr<DataManager> data_manager,
        DesignerRowType row_type,
        WhiskerToolbox::TensorBuilders::ColumnRecipe const & recipe,
        QWidget * parent = nullptr);

    ~ColumnConfigDialog() override;

    /**
     * @brief Get the configured column recipe
     * @return The ColumnRecipe built from user selections
     */
    [[nodiscard]] WhiskerToolbox::TensorBuilders::ColumnRecipe getRecipe() const;

private slots:
    void _onSourceKeyChanged(int index);
    void _onOperationChanged(int index);
    void _onColumnNameEdited(QString const & text);
    void _updateAutoName();
    void _onAdvancedToggled(bool checked);
    void _onValidateClicked();
    void _onAdvancedJsonEdited();

private:
    void _setupUi();
    void _connectSignals();
    void _populateSourceKeys();
    void _populateOperations();
    void _applyRecipe(WhiskerToolbox::TensorBuilders::ColumnRecipe const & recipe);

    /// Build a pipeline JSON string from the current simple combo-box selection
    [[nodiscard]] std::string _buildJsonFromComboSelection() const;

    /// Check if the current advanced JSON represents a pipeline that cannot
    /// be described by the simple combo-box UX (multi-step, custom params, etc.)
    [[nodiscard]] bool _isAdvancedPipelineJson(std::string const & json) const;

    std::shared_ptr<DataManager> _data_manager;
    DesignerRowType _row_type;
    bool _auto_name{true}; ///< Auto-generate column name from source + operation

    // UI
    QVBoxLayout * _layout{nullptr};

    // Source
    QLabel * _source_label{nullptr};
    QComboBox * _source_combo{nullptr};
    QLabel * _source_type_label{nullptr};

    // Operation
    QLabel * _operation_label{nullptr};
    QComboBox * _operation_combo{nullptr};

    // Parameters (stacked for different operation types)
    QStackedWidget * _param_stack{nullptr};

    // Offset parameter page (for AnalogSampleAtOffset)
    QWidget * _offset_page{nullptr};
    QLabel * _offset_label{nullptr};
    QDoubleSpinBox * _offset_spin{nullptr};

    // Empty page (for operations with no parameters)
    QWidget * _empty_page{nullptr};

    // --- Advanced Pipeline JSON section ---
    QGroupBox * _advanced_group{nullptr};
    QTextEdit * _advanced_json_edit{nullptr};
    QPushButton * _validate_btn{nullptr};
    QPushButton * _request_tv2_btn{nullptr}; ///< Placeholder for Phase 6.4 OperationContext
    QLabel * _validation_label{nullptr};
    bool _use_advanced_json{false}; ///< True when user has activated advanced mode
    bool _syncing_json{false};     ///< Guard against recursive update loops

    // Column name
    QLabel * _name_label{nullptr};
    QLineEdit * _name_edit{nullptr};
};

#endif // COLUMN_CONFIG_DIALOG_HPP
