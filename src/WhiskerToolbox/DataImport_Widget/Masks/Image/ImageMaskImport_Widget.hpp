#ifndef IMAGE_MASK_IMPORT_WIDGET_HPP
#define IMAGE_MASK_IMPORT_WIDGET_HPP

/**
 * @file ImageMaskImport_Widget.hpp
 * @brief Widget for configuring image-based mask data import options
 * 
 * This widget provides UI controls for loading mask data from binary image files.
 * Each image file represents a mask for a specific frame.
 */

#include <QWidget>
#include <QString>

#include "nlohmann/json.hpp"

namespace Ui {
class ImageMaskImport_Widget;
}

/**
 * @brief Widget for image mask import configuration
 * 
 * Allows users to configure:
 * - Directory containing mask images
 * - File pattern for matching image files
 * - Filename prefix
 * - Frame number padding
 * - Threshold value for binary mask creation
 * - Mask inversion option
 */
class ImageMaskImport_Widget : public QWidget {
    Q_OBJECT
public:
    explicit ImageMaskImport_Widget(QWidget * parent = nullptr);
    ~ImageMaskImport_Widget() override;

signals:
    /**
     * @brief Emitted when user requests to load image masks
     * @param format Format string (always "image" for this widget)
     * @param config JSON configuration containing all loading options
     */
    void loadImageMaskRequested(QString format, nlohmann::json config);

private slots:
    void _onBrowseDirectoryClicked();
    void _onLoadButtonClicked();

private:
    Ui::ImageMaskImport_Widget * ui;
};

#endif // IMAGE_MASK_IMPORT_WIDGET_HPP
