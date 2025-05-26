#ifndef IMAGEMASKSAVER_WIDGET_HPP
#define IMAGEMASKSAVER_WIDGET_HPP

#include <QWidget>
#include "DataManager/Masks/IO/Image/Mask_Data_Image.hpp"

namespace Ui {
class ImageMaskSaver_Widget;
}

class ImageMaskSaver_Widget : public QWidget {
    Q_OBJECT

public:
    explicit ImageMaskSaver_Widget(QWidget *parent = nullptr);
    ~ImageMaskSaver_Widget() override;

signals:
    void saveImageMaskRequested(ImageMaskSaverOptions options);

private slots:
    void _onBrowseDirectoryButtonClicked();
    void _onSaveButtonClicked();

private:
    Ui::ImageMaskSaver_Widget *ui;
};

#endif // IMAGEMASKSAVER_WIDGET_HPP 