#ifndef POINT_IMPORT_WIDGET_HPP
#define POINT_IMPORT_WIDGET_HPP

/**
 * @file PointImport_Widget.hpp
 * @brief Widget for importing point data into DataManager
 * 
 * This widget provides a unified interface for loading point data from
 * various file formats. Currently supports CSV format.
 * 
 * ## Usage
 * 
 * The widget is typically created by DataImportTypeRegistry when user
 * selects "PointData" type. It handles:
 * - Format selection (CSV, etc.)
 * - Format-specific options via stacked sub-widgets
 * - Coordinate scaling via Scaling_Widget
 * - Data naming and registration with DataManager
 */

#include "DataManager/Points/IO/CSV/Point_Data_CSV.hpp"

#include <QWidget>

#include <memory>

class DataManager;

namespace Ui {
class PointImport_Widget;
}

/**
 * @brief Widget for importing point data
 * 
 * Provides:
 * - Data name input
 * - Format selector (CSV)
 * - Format-specific options (via stacked widget)
 * - Image scaling configuration
 */
class PointImport_Widget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct the PointImport_Widget
     * @param data_manager DataManager for storing imported data
     * @param parent Parent widget
     */
    explicit PointImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~PointImport_Widget() override;

signals:
    /**
     * @brief Emitted when point data import completes successfully
     * @param data_key Key of the imported data in DataManager
     * @param data_type Type string ("PointData")
     */
    void importCompleted(QString const & data_key, QString const & data_type);

private:
    Ui::PointImport_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

    /**
     * @brief Load point data from a CSV file
     * @param options CSV loader options including filename
     */
    void _loadSingleCSVFile(CSVPointLoaderOptions const & options);

private slots:
    /**
     * @brief Handle format type combo box change
     * @param index New selected index
     */
    void _onLoaderTypeChanged(int index);
    
    /**
     * @brief Handle CSV load request from sub-widget
     * @param options Configured options (filename will be prompted)
     */
    void _handleSingleCSVLoadRequested(CSVPointLoaderOptions options);
};

#endif // POINT_IMPORT_WIDGET_HPP
