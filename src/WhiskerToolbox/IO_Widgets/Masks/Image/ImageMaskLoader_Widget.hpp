#ifndef IMAGE_MASK_LOADER_WIDGET_HPP
#define IMAGE_MASK_LOADER_WIDGET_HPP

#include <QWidget>
#include <QString>
#include "DataManager/Masks/IO/Image/Mask_Data_Image.hpp"

namespace Ui {
class ImageMaskLoader_Widget;
}

class ImageMaskLoader_Widget : public QWidget {
    Q_OBJECT
public:
    explicit ImageMaskLoader_Widget(QWidget *parent = nullptr);
    ~ImageMaskLoader_Widget() override;

signals:
    void loadImageMaskRequested(ImageMaskLoaderOptions options);

private slots:
    void _onBrowseDirectoryClicked();
    void _onLoadButtonClicked();

private:
    Ui::ImageMaskLoader_Widget *ui;
};

#endif // IMAGE_MASK_LOADER_WIDGET_HPP 