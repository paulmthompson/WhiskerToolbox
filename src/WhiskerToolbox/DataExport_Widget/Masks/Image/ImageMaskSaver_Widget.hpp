#ifndef IMAGEMASKSAVER_WIDGET_HPP
#define IMAGEMASKSAVER_WIDGET_HPP

#include <QWidget>
#include <QString>
#include "nlohmann/json.hpp"

namespace Ui {
class ImageMaskSaver_Widget;
}

class ImageMaskSaver_Widget : public QWidget {
    Q_OBJECT

public:
    explicit ImageMaskSaver_Widget(QWidget *parent = nullptr);
    ~ImageMaskSaver_Widget() override;

signals:
    void saveImageMaskRequested(QString format, nlohmann::json config);

private slots:
    void _onBrowseDirectoryButtonClicked();
    void _onSaveButtonClicked();

private:
    Ui::ImageMaskSaver_Widget *ui;
};

#endif // IMAGEMASKSAVER_WIDGET_HPP 