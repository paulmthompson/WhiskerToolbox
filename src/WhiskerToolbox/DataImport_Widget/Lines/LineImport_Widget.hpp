#ifndef LINE_IMPORT_WIDGET_HPP
#define LINE_IMPORT_WIDGET_HPP

/**
 * @file LineImport_Widget.hpp
 * @brief Widget for importing line data into DataManager
 * 
 * This widget provides a unified interface for loading line data from
 * various file formats. Currently supports HDF5, CSV, Binary, and LMDB formats.
 * 
 * ## Usage
 * 
 * The widget is typically created by DataImportTypeRegistry when user
 * selects "LineData" type. It handles:
 * - Format selection (HDF5, CSV, Binary, LMDB)
 * - Format-specific options via stacked sub-widgets
 * - Coordinate scaling via Scaling_Widget
 * - Data naming and registration with DataManager
 * 
 * ## Supported Formats
 * 
 * - **HDF5**: Single or multiple HDF5 files containing sparse line data
 * - **CSV**: Single file with all timestamps or directory of per-frame files
 * - **Binary**: Cap'n Proto binary serialized line data
 * - **LMDB**: Lightning Memory-Mapped Database format (placeholder)
 */

#include <QWidget>
#include <QString>

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class DataManager;

namespace Ui {
class LineImport_Widget;
}

/**
 * @brief Widget for importing line data
 * 
 * Provides:
 * - Data name input
 * - Format selector (HDF5, CSV, Binary, LMDB)
 * - Format-specific options (via stacked widget)
 * - Image scaling configuration
 */
class LineImport_Widget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct the LineImport_Widget
     * @param data_manager DataManager for storing imported data
     * @param parent Parent widget
     */
    explicit LineImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~LineImport_Widget() override;

signals:
    /**
     * @brief Emitted when line data import completes successfully
     * @param data_key Key of the imported data in DataManager
     * @param data_type Type string ("LineData")
     */
    void importCompleted(QString const & data_key, QString const & data_type);

private:
    Ui::LineImport_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

    /**
     * @brief Load line data from a single HDF5 file
     * @param filename Path to the HDF5 file
     * @param line_suffix Optional suffix to append to line key
     */
    void _loadSingleHDF5LineFile(std::string const & filename, std::string const & line_suffix = "");
    
    /**
     * @brief Load line data from multiple HDF5 files matching a pattern
     * @param directory_path Directory containing HDF5 files
     * @param pattern Filename pattern for matching
     */
    void _loadMultiHDF5LineFiles(QString const & directory_path, QString const & pattern);

    /**
     * @brief Load line data from a single binary file
     * @param filepath Path to the binary file
     */
    void _loadSingleBinaryFile(QString const & filepath);

private slots:
    /**
     * @brief Handle format type combo box change
     * @param index New selected index
     */
    void _onLoaderTypeChanged(int index);
    
    /**
     * @brief Handle single HDF5 load request from sub-widget
     */
    void _handleSingleHDF5LoadRequested();
    
    /**
     * @brief Handle multi HDF5 load request from sub-widget
     * @param pattern Filename pattern for matching
     */
    void _handleMultiHDF5LoadRequested(QString pattern);

    /**
     * @brief Handle single CSV file load request from sub-widget
     * @param format Format string
     * @param config JSON configuration for loading
     */
    void _handleSingleCSVLoadRequested(QString format, nlohmann::json config);

    /**
     * @brief Handle multi CSV file load request from sub-widget
     * @param format Format string
     * @param config JSON configuration for loading
     */
    void _handleMultiCSVLoadRequested(QString format, nlohmann::json config);

    /**
     * @brief Handle binary file load request from sub-widget
     * @param filepath Path to the binary file
     */
    void _handleBinaryLoadRequested(QString filepath);
};

#endif // LINE_IMPORT_WIDGET_HPP
