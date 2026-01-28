#ifndef MASK_IMPORT_WIDGET_HPP
#define MASK_IMPORT_WIDGET_HPP

/**
 * @file MaskImport_Widget.hpp
 * @brief Widget for importing mask data into DataManager
 * 
 * This widget provides a unified interface for loading mask data from
 * various file formats. Currently supports HDF5 and image formats.
 * 
 * ## Usage
 * 
 * The widget is typically created by DataImportTypeRegistry when user
 * selects "MaskData" type. It handles:
 * - Format selection (HDF5, Image)
 * - Format-specific options via stacked sub-widgets
 * - Coordinate scaling via Scaling_Widget
 * - Data naming and registration with DataManager
 * 
 * ## Supported Formats
 * 
 * - **HDF5**: Single or multiple HDF5 files containing sparse mask data
 * - **Image**: Directory of binary images representing frame-by-frame masks
 */

#include <QWidget>
#include <QString>

#include "nlohmann/json.hpp"

#include <memory>
#include <string>

class DataManager;

namespace Ui {
class MaskImport_Widget;
}

/**
 * @brief Widget for importing mask data
 * 
 * Provides:
 * - Data name input
 * - Format selector (HDF5, Image)
 * - Format-specific options (via stacked widget)
 * - Image scaling configuration
 */
class MaskImport_Widget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct the MaskImport_Widget
     * @param data_manager DataManager for storing imported data
     * @param parent Parent widget
     */
    explicit MaskImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~MaskImport_Widget() override;

signals:
    /**
     * @brief Emitted when mask data import completes successfully
     * @param data_key Key of the imported data in DataManager
     * @param data_type Type string ("MaskData")
     */
    void importCompleted(QString const & data_key, QString const & data_type);

private:
    Ui::MaskImport_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

    /**
     * @brief Load mask data from a single HDF5 file
     * @param filename Path to the HDF5 file
     * @param mask_suffix Optional suffix to append to mask key
     */
    void _loadSingleHDF5MaskFile(std::string const & filename, std::string const & mask_suffix = "");
    
    /**
     * @brief Load mask data from multiple HDF5 files matching a pattern
     * @param directory_path Directory containing HDF5 files
     * @param pattern Filename pattern for matching
     */
    void _loadMultiHDF5MaskFiles(QString const & directory_path, QString const & pattern);

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
     * @brief Handle image mask load request from sub-widget
     * @param format Format string
     * @param config JSON configuration for loading
     */
    void _handleImageMaskLoadRequested(QString format, nlohmann::json config);
};

#endif // MASK_IMPORT_WIDGET_HPP
