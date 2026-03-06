/**
 * @file CSVMaskSaver_Widget.hpp
 * @brief Widget for saving MaskData to CSV files using Run-Length Encoding (RLE)
 */
#ifndef CSVMASKSAVER_WIDGET_HPP
#define CSVMASKSAVER_WIDGET_HPP

#include "nlohmann/json.hpp"

#include <QString>
#include <QWidget>

namespace Ui {
class CSVMaskSaver_Widget;
}

class CSVMaskSaver_Widget : public QWidget {
    Q_OBJECT

public:
    explicit CSVMaskSaver_Widget(QWidget * parent = nullptr);
    ~CSVMaskSaver_Widget() override;

signals:
    void saveCSVMaskRequested(QString format, nlohmann::json config);

private slots:
    void _onBrowseDirectoryButtonClicked();
    void _onSaveButtonClicked();

private:
    Ui::CSVMaskSaver_Widget * ui;
};

#endif// CSVMASKSAVER_WIDGET_HPP
