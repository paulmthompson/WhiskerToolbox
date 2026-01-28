#ifndef MASK_INSPECTOR_HPP
#define MASK_INSPECTOR_HPP

/**
 * @file MaskInspector.hpp
 * @brief Inspector widget for MaskData
 * 
 * MaskInspector provides inspection capabilities for MaskData objects.
 * It wraps the existing Mask_Widget to reuse its functionality while
 * conforming to the IDataInspector interface.
 * 
 * ## Features
 * - Mask data table with frame and mask information
 * - Group filtering
 * - Context menu for move/copy operations
 * - Export to image and HDF5 formats
 * - Frame navigation via double-click
 * 
 * @see Mask_Widget for the underlying implementation
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

#include <memory>

class Mask_Widget;

/**
 * @brief Inspector widget for MaskData
 * 
 * Wraps Mask_Widget to provide IDataInspector interface while reusing
 * existing functionality for mask data inspection and management.
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

private:
    void _setupUi();
    void _connectSignals();

    std::unique_ptr<Mask_Widget> _mask_widget;
};

#endif // MASK_INSPECTOR_HPP
