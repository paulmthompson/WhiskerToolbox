#ifndef CSV_LINE_IMPORT_WIDGET_HPP
#define CSV_LINE_IMPORT_WIDGET_HPP

/**
 * @file CSVLineImport_Widget.hpp
 * @brief Widget for configuring CSV line data import options
 * 
 * This widget provides UI controls for loading line data from CSV files.
 * Supports both single file mode (all timestamps in one file) and
 * multi-file mode (one CSV file per timestamp).
 */

#include <QWidget>
#include <QString>

#include "nlohmann/json.hpp"

namespace Ui {
class CSVLineImport_Widget;
}

/**
 * @brief Widget for CSV line import configuration
 * 
 * Allows users to configure:
 * - Load mode (single file vs multi-file)
 * - File/directory path
 * - Delimiter settings
 * - Column indices for X/Y coordinates
 * - Header row handling
 * - Coordinate delimiter (for single-file mode)
 * - Header identifier (for single-file mode)
 */
class CSVLineImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVLineImport_Widget(QWidget * parent = nullptr);
    ~CSVLineImport_Widget() override;

signals:
    /**
     * @brief Emitted when user requests to load from a single CSV file
     * @param format Format string (always "csv" for this widget)
     * @param config JSON configuration containing all loading options
     */
    void loadSingleFileCSVRequested(QString format, nlohmann::json config);
    
    /**
     * @brief Emitted when user requests to load from multiple CSV files
     * @param format Format string (always "csv" for this widget)
     * @param config JSON configuration containing all loading options
     */
    void loadMultiFileCSVRequested(QString format, nlohmann::json config);

private slots:
    void _onLoadModeChanged();
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();

private:
    Ui::CSVLineImport_Widget * ui;
    void _updateUIForLoadMode();
};

#endif // CSV_LINE_IMPORT_WIDGET_HPP
