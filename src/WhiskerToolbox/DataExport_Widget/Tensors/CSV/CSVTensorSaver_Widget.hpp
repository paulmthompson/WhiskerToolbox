/**
 * @file CSVTensorSaver_Widget.hpp
 * @brief Widget for configuring CSV tensor data export options
 */

#ifndef CSV_TENSOR_SAVER_WIDGET_HPP
#define CSV_TENSOR_SAVER_WIDGET_HPP

#include "IO/formats/CSV/tensors/Tensor_Data_CSV.hpp"

#include <QWidget>

namespace Ui {
class CSVTensorSaver_Widget;
}

/**
 * @brief Widget for configuring CSV tensor export options
 *
 * Provides UI controls for delimiter, precision, and header settings.
 * Emits saveTensorCSVRequested when the user clicks save.
 * Filename and parent_dir are set by the calling widget (inspector).
 */
class CSVTensorSaver_Widget : public QWidget {
    Q_OBJECT

public:
    explicit CSVTensorSaver_Widget(QWidget * parent = nullptr);
    ~CSVTensorSaver_Widget() override;

signals:
    void saveTensorCSVRequested(CSVTensorSaverOptions options);

private slots:
    void _onSaveActionButtonClicked();
    static void _onSaveHeaderCheckboxToggled(bool checked);
    void _updatePrecisionExample(int precision);

private:
    Ui::CSVTensorSaver_Widget * ui;
    [[nodiscard]] CSVTensorSaverOptions _getOptionsFromUI() const;
    void _updatePrecisionLabelText(int precision);
};

#endif// CSV_TENSOR_SAVER_WIDGET_HPP
