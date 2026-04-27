#ifndef TENSOR_IMPORT_WIDGET_HPP
#define TENSOR_IMPORT_WIDGET_HPP

/**
 * @file TensorImport_Widget.hpp
 * @brief Widget for importing tensor data into DataManager
 *
 * This widget provides a unified interface for importing tensor data
 * from multiple file formats (NumPy .npy and CSV).
 * Uses a format selector combo box and stacked widget to switch between
 * format-specific import sub-widgets.
 */

#include <QWidget>

#include <memory>

struct CSVTensorLoaderOptions;
class DataManager;

namespace Ui {
class TensorImport_Widget;
}

/**
 * @brief Widget for importing tensor data from multiple formats
 *
 * Provides:
 * - Data name input
 * - Format selector (NumPy, CSV)
 * - Format-specific import options via stacked widget
 */
class TensorImport_Widget : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct the TensorImport_Widget
     * @param data_manager DataManager for storing imported data
     * @param parent Parent widget
     */
    explicit TensorImport_Widget(std::shared_ptr<DataManager> data_manager, QWidget * parent = nullptr);
    ~TensorImport_Widget() override;

signals:
    /**
     * @brief Emitted when tensor data import completes successfully
     * @param data_key Key of the imported data in DataManager
     * @param data_type Type string ("TensorData")
     */
    void importCompleted(QString const & data_key, QString const & data_type);

private:
    Ui::TensorImport_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;

private slots:
    void _onLoaderTypeChanged(int index);
    void _loadNumpyArray();
    void _handleCSVLoadRequested(CSVTensorLoaderOptions const & options);
};

#endif// TENSOR_IMPORT_WIDGET_HPP
