#ifndef TENSOR_IMPORT_WIDGET_HPP
#define TENSOR_IMPORT_WIDGET_HPP

/**
 * @file TensorImport_Widget.hpp
 * @brief Widget for importing tensor data into DataManager
 * 
 * This widget provides an interface for loading tensor data from
 * NumPy (.npy) files.
 */

#include <QWidget>
#include <memory>

class DataManager;

namespace Ui {
class TensorImport_Widget;
}

/**
 * @brief Widget for importing tensor data
 * 
 * Provides:
 * - Data name input
 * - NumPy file loading
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
    /**
     * @brief Handle numpy array loading
     */
    void _loadNumpyArray();
};

#endif // TENSOR_IMPORT_WIDGET_HPP
