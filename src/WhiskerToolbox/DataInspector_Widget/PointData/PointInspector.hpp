#ifndef POINT_INSPECTOR_HPP
#define POINT_INSPECTOR_HPP

/**
 * @file PointInspector.hpp
 * @brief Inspector widget for PointData
 * 
 * PointInspector provides inspection capabilities for PointData objects.
 * It wraps the existing Point_Widget to reuse its functionality while
 * conforming to the IDataInspector interface.
 * 
 * ## Features
 * - Point data table with frame, coordinates, and group information
 * - Group filtering
 * - Context menu for move/copy operations
 * - Export to CSV
 * - Frame navigation via double-click
 * 
 * @see Point_Widget for the underlying implementation
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

#include <memory>

class Point_Widget;

/**
 * @brief Inspector widget for PointData
 * 
 * Wraps Point_Widget to provide IDataInspector interface while reusing
 * existing functionality for point data inspection and management.
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

private:
    void _setupUi();
    void _connectSignals();

    std::unique_ptr<Point_Widget> _point_widget;
};

#endif // POINT_INSPECTOR_HPP
