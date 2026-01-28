#ifndef HDF5_LINE_IMPORT_WIDGET_HPP
#define HDF5_LINE_IMPORT_WIDGET_HPP

/**
 * @file HDF5LineImport_Widget.hpp
 * @brief Widget for configuring HDF5 line data import options
 * 
 * This widget provides UI controls for loading line data from HDF5 files.
 * Supports both single file loading and batch loading with pattern matching.
 */

#include <QWidget>
#include <QString>

namespace Ui {
class HDF5LineImport_Widget;
}

/**
 * @brief Widget for HDF5 line import configuration
 * 
 * Allows users to:
 * - Load a single HDF5 line file
 * - Load multiple HDF5 line files matching a pattern
 */
class HDF5LineImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit HDF5LineImport_Widget(QWidget * parent = nullptr);
    ~HDF5LineImport_Widget() override;

signals:
    /**
     * @brief Emitted when user requests to load a single HDF5 line file
     */
    void loadSingleHDF5LineRequested();
    
    /**
     * @brief Emitted when user requests to load multiple HDF5 line files
     * @param pattern Filename pattern for matching files (e.g., "*.h5")
     */
    void loadMultiHDF5LineRequested(QString pattern);

private:
    Ui::HDF5LineImport_Widget * ui;
};

#endif // HDF5_LINE_IMPORT_WIDGET_HPP
