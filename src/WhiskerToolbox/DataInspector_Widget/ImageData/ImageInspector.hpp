#ifndef IMAGE_INSPECTOR_HPP
#define IMAGE_INSPECTOR_HPP

/**
 * @file ImageInspector.hpp
 * @brief Inspector widget for MediaData (images/video)
 * 
 * ImageInspector provides inspection capabilities for MediaData objects.
 * 
 * ## Features
 * - Data change callbacks for image/media data
 * 
 * Note: Image table view is provided by ImageDataView in the view panel.
 * 
 * @see BaseInspector for the base class
 * @see ImageDataView for the table view
 */

#include "DataInspector_Widget/Inspectors/BaseInspector.hpp"

/**
 * @brief Inspector widget for MediaData (images/video)
 * 
 * Provides callback management for image/media data inspection.
 * The actual table view is handled by ImageDataView.
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

private slots:
    /**
     * @brief Handle data change notifications
     */
    void _onDataChanged();

private:
    void _assignCallbacks();
};

#endif // IMAGE_INSPECTOR_HPP
