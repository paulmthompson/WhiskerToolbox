#ifndef TENSOR_INSPECTOR_HPP
#define TENSOR_INSPECTOR_HPP

/**
 * @file TensorInspector.hpp
 * @brief Inspector widget for TensorData
 * 
 * TensorInspector provides inspection capabilities for TensorData objects.
 * 
 * ## Features
 * - Data change callbacks for tensor data
 * 
 * Note: Tensor table view is provided by TensorDataView in the view panel.
 * 
 * @see BaseInspector for the base class
 * @see TensorDataView for the table view
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

/**
 * @brief Inspector widget for TensorData
 * 
 * Provides callback management for tensor data inspection.
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

private slots:
    /**
     * @brief Handle data change notifications
     */
    void _onDataChanged();

private:
    void _assignCallbacks();
};

#endif // TENSOR_INSPECTOR_HPP
