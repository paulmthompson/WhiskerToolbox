/**
 * @file CSVTensorImport_Widget.hpp
 * @brief Widget for configuring CSV tensor data import options
 */

#ifndef CSV_TENSOR_IMPORT_WIDGET_HPP
#define CSV_TENSOR_IMPORT_WIDGET_HPP

#include <QWidget>

#include "IO/formats/CSV/tensors/Tensor_Data_CSV.hpp"

namespace Ui {
class CSVTensorImport_Widget;
}

/**
 * @brief Widget for CSV tensor import configuration
 *
 * Provides UI controls for:
 * - File selection (browse dialog)
 * - Delimiter configuration
 * - Header row handling
 *
 * Row type is auto-detected from file content by the loader.
 */
class CSVTensorImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit CSVTensorImport_Widget(QWidget * parent = nullptr);
    ~CSVTensorImport_Widget() override;

signals:
    /**
     * @brief Emitted when user requests to load tensor data from CSV
     * @param options The configured loader options
     */
    void loadTensorCSVRequested(CSVTensorLoaderOptions options);

private slots:
    void _onBrowseButtonClicked();
    void _onLoadButtonClicked();

private:
    Ui::CSVTensorImport_Widget * ui;
};

#endif// CSV_TENSOR_IMPORT_WIDGET_HPP
