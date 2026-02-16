#ifndef CSV_EXPLORER_WIDGET_HPP
#define CSV_EXPLORER_WIDGET_HPP

/**
 * @file CsvExplorer_Widget.hpp
 * @brief Widget for browsing and previewing CSV / delimited text files
 *
 * CsvExplorer_Widget provides configurable parsing and preview of CSV and
 * other delimited text files. Users can specify:
 * - Column delimiter (comma, tab, semicolon, pipe, space, or custom)
 * - Number of header lines to skip
 * - Whether to use the first header line as column names
 * - Quote character for quoted fields
 * - Whitespace trimming
 *
 * ## Features
 *
 * - File selection via dialog
 * - Configurable parsing parameters with live preview update
 * - Metadata panel showing file size, line count, columns, header info
 * - Lazy-loaded data preview table
 *
 * ## Integration
 *
 * The widget is registered with DataImportTypeRegistry as "CsvExplorer",
 * appearing in the Data Import widget for CSV file exploration.
 */

#include <QWidget>
#include <QString>

#include <memory>

// Forward declarations
class DataManager;
class QTableView;
class CsvDatasetPreviewModel;
struct CsvFileInfo;
struct CsvParseConfig;

namespace Ui {
class CsvExplorer_Widget;
}

/**
 * @brief Widget for browsing CSV/delimited text files
 *
 * Provides:
 * - File browse and path display
 * - Parse configuration panel (delimiter, header lines, quote char, etc.)
 * - Metadata panel (file size, line count, column count, detected structure)
 * - Lazy-loading data preview table that updates on configuration changes
 */
class CsvExplorer_Widget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct the CsvExplorer_Widget
     *
     * @param data_manager DataManager for future data import integration
     * @param parent Parent widget
     */
    explicit CsvExplorer_Widget(
        std::shared_ptr<DataManager> data_manager = nullptr,
        QWidget * parent = nullptr);

    ~CsvExplorer_Widget() override;

    /**
     * @brief Load and display a CSV file
     * @param file_path Path to the CSV file
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
     * @brief Handle apply button click
     */
    void _onApplyClicked();

    /**
     * @brief Handle any parse configuration change
     *
     * Rebuilds the parse config from UI controls and reloads the preview.
     */
    void _onConfigChanged();

private:
    Ui::CsvExplorer_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    QString _current_file_path;

    // Preview model for lazy-loading CSV contents
    CsvDatasetPreviewModel * _preview_model;

    /**
     * @brief Clear all displayed information
     */
    void _clearDisplay();

    /**
     * @brief Build a CsvParseConfig from the current UI controls
     */
    [[nodiscard]] CsvParseConfig _buildConfigFromUI() const;

    /**
     * @brief Update the metadata/info panel with computed file information
     * @param info Computed CSV file information
     */
    void _updateInfoPanel(CsvFileInfo const & info);

    /**
     * @brief Populate the column delimiter combo box
     */
    void _populateDelimiterCombo();
};

#endif // CSV_EXPLORER_WIDGET_HPP
