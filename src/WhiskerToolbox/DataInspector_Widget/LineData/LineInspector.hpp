#ifndef LINE_INSPECTOR_HPP
#define LINE_INSPECTOR_HPP

/**
 * @file LineInspector.hpp
 * @brief Inspector widget for LineData
 * 
 * LineInspector provides inspection capabilities for LineData objects.
 * It wraps the existing Line_Widget to reuse its functionality while
 * conforming to the IDataInspector interface.
 * 
 * ## Features
 * - Line data table with frame and polyline information
 * - Group filtering
 * - Context menu for move/copy operations
 * - Export to CSV and binary formats
 * - Frame navigation via double-click
 * 
 * @see Line_Widget for the underlying implementation
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

#include <memory>

class Line_Widget;

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

private:
    void _setupUi();
    void _connectSignals();

    std::unique_ptr<Line_Widget> _line_widget;
};

#endif // LINE_INSPECTOR_HPP
