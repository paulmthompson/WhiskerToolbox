#ifndef HDF5_MASK_IMPORT_WIDGET_HPP
#define HDF5_MASK_IMPORT_WIDGET_HPP

/**
 * @file HDF5MaskImport_Widget.hpp
 * @brief Widget for configuring HDF5 mask data import options
 * 
 * This widget provides UI controls for loading mask data from HDF5 files.
 * Supports both single file loading and batch loading with pattern matching.
 */

#include <QWidget>
#include <QString>

namespace Ui {
class HDF5MaskImport_Widget;
}

/**
 * @brief Widget for HDF5 mask import configuration
 * 
 * Allows users to:
 * - Load a single HDF5 mask file
 * - Load multiple HDF5 mask files matching a pattern
 */
class HDF5MaskImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit HDF5MaskImport_Widget(QWidget * parent = nullptr);
    ~HDF5MaskImport_Widget() override;

signals:
    /**
     * @brief Emitted when user requests to load a single HDF5 mask file
     */
    void loadSingleHDF5MaskRequested();
    
    /**
     * @brief Emitted when user requests to load multiple HDF5 mask files
     * @param pattern Filename pattern for matching files (e.g., "*.h5")
     */
    void loadMultiHDF5MaskRequested(QString pattern);

private:
    Ui::HDF5MaskImport_Widget * ui;
};

#endif // HDF5_MASK_IMPORT_WIDGET_HPP
