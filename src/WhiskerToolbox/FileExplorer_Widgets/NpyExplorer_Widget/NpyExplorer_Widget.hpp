#ifndef NPY_EXPLORER_WIDGET_HPP
#define NPY_EXPLORER_WIDGET_HPP

/**
 * @file NpyExplorer_Widget.hpp
 * @brief Widget for browsing and previewing NumPy .npy files
 * 
 * NpyExplorer_Widget provides a metadata overview and data preview for NumPy
 * .npy files. Users can browse for a file and see:
 * - Data type (dtype), shape, byte order, memory layout
 * - A lazy-loaded preview table of the array contents
 * 
 * ## Features
 * 
 * - File selection via dialog
 * - Metadata panel showing dtype, shape, element count, byte order, memory order
 * - Data preview table with lazy loading for efficient memory usage
 * - Selection signals for downstream import widgets
 * 
 * ## Integration
 * 
 * The widget is registered with DataImportTypeRegistry as "NpyExplorer",
 * appearing in the Data Import widget for .npy file exploration.
 */

#include <QWidget>
#include <QString>

#include <memory>

// Forward declarations
class DataManager;
class QTableView;
class NpyDatasetPreviewModel;
struct NpyFileInfo;

namespace Ui {
class NpyExplorer_Widget;
}

/**
 * @brief Widget for browsing NumPy .npy files
 * 
 * Provides:
 * - File browse and path display
 * - Metadata panel (dtype, shape, byte order, memory order, element count)
 * - Lazy-loading data preview table
 */
class NpyExplorer_Widget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the NpyExplorer_Widget
     * 
     * @param data_manager DataManager for future data import integration
     * @param parent Parent widget
     */
    explicit NpyExplorer_Widget(
        std::shared_ptr<DataManager> data_manager = nullptr,
        QWidget * parent = nullptr);

    ~NpyExplorer_Widget() override;

    /**
     * @brief Load and display a .npy file
     * @param file_path Path to the .npy file
     * @return true if file was loaded successfully
     */
    bool loadFile(QString const & file_path);

    /**
     * @brief Get the currently loaded file path
     * @return File path, or empty if no file loaded
     */
    [[nodiscard]] QString currentFilePath() const { return _current_file_path; }

signals:
    /**
     * @brief Emitted when a file is successfully loaded
     * @param file_path Path to the loaded file
     */
    void fileLoaded(QString const & file_path);

    /**
     * @brief Emitted when an error occurs
     * @param message Error message
     */
    void errorOccurred(QString const & message);

private slots:
    /**
     * @brief Handle browse button click
     */
    void _onBrowseClicked();

    /**
     * @brief Handle refresh button click
     */
    void _onRefreshClicked();

private:
    Ui::NpyExplorer_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    QString _current_file_path;

    // Preview model for lazy-loading array contents
    NpyDatasetPreviewModel * _preview_model;

    /**
     * @brief Clear all displayed information
     */
    void _clearDisplay();

    /**
     * @brief Update the metadata panel with file info
     * @param info Parsed .npy file information
     */
    void _updateInfoPanel(NpyFileInfo const & info);
};

#endif // NPY_EXPLORER_WIDGET_HPP
