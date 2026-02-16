#ifndef BINARY_EXPLORER_WIDGET_HPP
#define BINARY_EXPLORER_WIDGET_HPP

/**
 * @file BinaryExplorer_Widget.hpp
 * @brief Widget for browsing and previewing raw binary data files
 * 
 * BinaryExplorer_Widget provides configurable parsing and preview of raw binary
 * files. Users can specify:
 * - Header size (bytes to skip at the start of the file)
 * - Data format (int8, int16, int32, int64, uint8, uint16, uint32, uint64, float32, float64)
 * - Number of channels (interleaved columns)
 * - Byte order (little-endian or big-endian)
 * - Bitwise expansion mode (expand integer values into individual bit columns)
 * 
 * The bitwise mode is particularly useful for digital acquisition systems where
 * each sample is a uint16_t with individual bits representing high/low states
 * on different digital input channels.
 * 
 * ## Features
 * 
 * - File selection via dialog
 * - Configurable parsing parameters with live preview update
 * - Metadata panel showing file size, element count, computed rows/columns
 * - Lazy-loaded data preview table
 * - Bitwise expansion for digital event data
 * 
 * ## Integration
 * 
 * The widget is registered with DataImportTypeRegistry as "BinaryExplorer",
 * appearing in the Data Import widget for raw binary file exploration.
 */

#include <QWidget>
#include <QString>

#include <memory>

// Forward declarations
class DataManager;
class QTableView;
class BinaryDatasetPreviewModel;
struct BinaryFileInfo;
struct BinaryParseConfig;

namespace Ui {
class BinaryExplorer_Widget;
}

/**
 * @brief Widget for browsing raw binary data files
 * 
 * Provides:
 * - File browse and path display
 * - Parse configuration panel (data type, header size, channels, byte order, bitwise)
 * - Metadata panel (file size, element count, rows, columns, remainder bytes)
 * - Lazy-loading data preview table that updates on configuration changes
 */
class BinaryExplorer_Widget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the BinaryExplorer_Widget
     * 
     * @param data_manager DataManager for future data import integration
     * @param parent Parent widget
     */
    explicit BinaryExplorer_Widget(
        std::shared_ptr<DataManager> data_manager = nullptr,
        QWidget * parent = nullptr);

    ~BinaryExplorer_Widget() override;

    /**
     * @brief Load and display a binary file
     * @param file_path Path to the binary file
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
     * @brief Handle refresh / apply button click
     */
    void _onApplyClicked();

    /**
     * @brief Handle any parse configuration change
     * 
     * Rebuilds the parse config from UI controls and reloads the preview.
     */
    void _onConfigChanged();

private:
    Ui::BinaryExplorer_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    QString _current_file_path;

    // Preview model for lazy-loading binary contents
    BinaryDatasetPreviewModel * _preview_model;

    /**
     * @brief Clear all displayed information
     */
    void _clearDisplay();

    /**
     * @brief Build a BinaryParseConfig from the current UI controls
     */
    [[nodiscard]] BinaryParseConfig _buildConfigFromUI() const;

    /**
     * @brief Update the metadata/info panel with computed file information
     * @param info Computed binary file information
     */
    void _updateInfoPanel(BinaryFileInfo const & info);

    /**
     * @brief Populate data type combo box
     */
    void _populateDataTypeCombo();

    /**
     * @brief Populate byte order combo box
     */
    void _populateByteOrderCombo();
};

#endif // BINARY_EXPLORER_WIDGET_HPP
