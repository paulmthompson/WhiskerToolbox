#ifndef IMAGE_INSPECTOR_HPP
#define IMAGE_INSPECTOR_HPP

/**
 * @file ImageInspector.hpp
 * @brief Inspector widget for MediaData (images/video)
 * 
 * ImageInspector provides inspection capabilities for MediaData objects.
 * It wraps the existing Image_Widget to reuse its functionality while
 * conforming to the IDataInspector interface.
 * 
 * ## Features
 * - Image data table with frame information
 * - Frame navigation via double-click
 * 
 * @see Image_Widget for the underlying implementation
 * @see BaseInspector for the base class
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

#include <memory>

class Image_Widget;

/**
 * @brief Inspector widget for MediaData (images/video)
 * 
 * Wraps Image_Widget to provide IDataInspector interface while reusing
 * existing functionality for image/media data inspection.
 */
class ImageInspector : public BaseInspector {
    Q_OBJECT

public:
    /**
     * @brief Construct the image inspector
     * @param data_manager Shared DataManager for data access
     * @param group_manager Optional GroupManager for group features (not used for images)
     * @param parent Parent widget
     */
    explicit ImageInspector(
        std::shared_ptr<DataManager> data_manager,
        GroupManager * group_manager = nullptr,
        QWidget * parent = nullptr);

    ~ImageInspector() override;

    // =========================================================================
    // IDataInspector Interface
    // =========================================================================

    void setActiveKey(std::string const & key) override;
    void removeCallbacks() override;
    void updateView() override;

    [[nodiscard]] DM_DataType getDataType() const override { return DM_DataType::Images; }
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("Image/Video"); }

    // Images don't typically have export in the same way
    [[nodiscard]] bool supportsExport() const override { return false; }
    
    // Images don't support group filtering
    [[nodiscard]] bool supportsGroupFiltering() const override { return false; }

private:
    void _setupUi();
    void _connectSignals();

    std::unique_ptr<Image_Widget> _image_widget;
};

#endif // IMAGE_INSPECTOR_HPP
