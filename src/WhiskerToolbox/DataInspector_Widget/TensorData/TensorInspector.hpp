#ifndef TENSOR_INSPECTOR_HPP
#define TENSOR_INSPECTOR_HPP

/**
 * @file TensorInspector.hpp
 * @brief Inspector widget for TensorData
 * 
 * TensorInspector provides inspection capabilities for TensorData objects.
 * It wraps the existing Tensor_Widget to reuse its functionality while
 * conforming to the IDataInspector interface.
 * 
 * ## Features
 * - Tensor shape and dimension display
 * - Export to CSV
 * 
 * @see Tensor_Widget for the underlying implementation
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

#include <memory>

class Tensor_Widget;

/**
 * @brief Inspector widget for TensorData
 * 
 * Wraps Tensor_Widget to provide IDataInspector interface while reusing
 * existing functionality for tensor data inspection and export.
 */
class TensorInspector : public BaseInspector {
    Q_OBJECT

public:
    /**
     * @brief Construct the tensor inspector
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group features (not used)
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

private:
    void _setupUi();
    void _connectSignals();

    std::unique_ptr<Tensor_Widget> _tensor_widget;
};

#endif // TENSOR_INSPECTOR_HPP
