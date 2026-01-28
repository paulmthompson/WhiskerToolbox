#ifndef CSV_POINT_IMPORT_WIDGET_HPP
#define CSV_POINT_IMPORT_WIDGET_HPP

/**
 * @file CSVPointImport_Widget.hpp
 * @brief Widget for configuring CSV point data import options
 * 
 * This widget provides UI controls for specifying CSV column mappings
 * and delimiter settings for importing point data.
 */

#include <QWidget>

struct CSVPointLoaderOptions;

namespace Ui {
class CSVPointImport_Widget;
}

/**
 * @brief Widget for CSV point import configuration
 * 
 * Allows users to configure:
 * - Column indices for frame, X, and Y coordinates
 * - CSV delimiter (comma, space, etc.)
 */
class CSVPointImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVPointImport_Widget(QWidget * parent = nullptr);
    ~CSVPointImport_Widget() override;

signals:
    /**
     * @brief Emitted when user requests to load a CSV file
     * @param options Configured loader options (filename will be set by parent)
     */
    void loadSingleCSVFileRequested(CSVPointLoaderOptions const & options);

private:
    Ui::CSVPointImport_Widget * ui;
};

#endif // CSV_POINT_IMPORT_WIDGET_HPP
