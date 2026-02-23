#ifndef TENSOR_INSPECTOR_HPP
#define TENSOR_INSPECTOR_HPP

/**
 * @file TensorInspector.hpp
 * @brief Inspector widget for TensorData with integrated TensorDesigner
 *
 * TensorInspector provides inspection capabilities for TensorData objects
 * and hosts the TensorDesigner panel for interactive column building.
 *
 * ## Features
 * - Data change callbacks for tensor data
 * - Embedded TensorDesigner for creating/editing lazy-column tensors
 * - SelectionContext integration for passive data awareness
 * - JSON save/load for column configurations
 * - CSV export for tensor data
 *
 * Note: Tensor table view is provided by TensorDataView in the view panel.
 *
 * @see BaseInspector for the base class
 * @see TensorDataView for the table view
 * @see TensorDesigner for the column-building panel
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

class SelectionContext;
class TensorDesigner;

namespace EditorLib {
class OperationContext;
}// namespace EditorLib

/**
 * @brief Inspector widget for TensorData with design mode
 *
 * Provides callback management for tensor data inspection and
 * hosts TensorDesigner for interactive lazy-column tensor building.
 * The actual table view is handled by TensorDataView.
 */
class TensorInspector : public BaseInspector {
    Q_OBJECT

public:
    /**
     * @brief Construct the tensor inspector
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group features (not used for tensors)
     * @param parent Parent widget
     */
    explicit TensorInspector(
            std::shared_ptr<DataManager> data_manager,
            GroupManager * group_manager = nullptr,
            QWidget * parent = nullptr);

    ~TensorInspector() override;

    // =========================================================================
    // IDataInspector Interface
    // =========================================================================

    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Tensor; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Tensor"); }

    [[nodiscard]] bool supportsExport() const override { return true; }
    [[nodiscard]] bool supportsGroupFiltering() const override { return false; }

    // =========================================================================
    // SelectionContext Integration
    // =========================================================================

    /**
     * @brief Set the SelectionContext for passive data awareness
     * @param context SelectionContext instance (can be nullptr)
     */
    void setSelectionContext(SelectionContext * context);

    /**
     * @brief Set the OperationContext for inter-widget pipeline requests
     * @param context OperationContext instance (can be nullptr)
     */
    void setOperationContext(EditorLib::OperationContext * context);

    /**
     * @brief Get the embedded TensorDesigner widget
     * @return Pointer to TensorDesigner, or nullptr if not created
     */
    [[nodiscard]] TensorDesigner * designer() const { return _designer; }

signals:
    /**
     * @brief Emitted when the designer creates or updates a tensor
     * @param key DataManager key of the tensor
     */
    void tensorCreated(QString const & key);

private slots:
    /**
     * @brief Handle data change notifications
     */
    void _onDataChanged();

    /**
     * @brief Handle tensor creation from designer
     */
    void _onTensorCreated(QString const & key);

private:
    void _setupDesignerUi();
    void _assignCallbacks();

    TensorDesigner * _designer{nullptr};
};

#endif// TENSOR_INSPECTOR_HPP
