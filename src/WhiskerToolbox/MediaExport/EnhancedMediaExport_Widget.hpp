#ifndef ENHANCED_MEDIA_EXPORT_WIDGET_HPP
#define ENHANCED_MEDIA_EXPORT_WIDGET_HPP

#include <QWidget>
#include <vector>
#include <memory>

class Media_Window;
class MediaDisplayCoordinator;

namespace Ui {
class EnhancedMediaExport_Widget;
}

enum class ExportLayout {
    Individual,      // Export each display separately
    HorizontalConcat, // Concatenate displays horizontally
    VerticalConcat   // Concatenate displays vertically
};

/**
 * @brief Enhanced MediaExport widget that can work with multiple media displays
 * 
 * This widget can export from multiple Media_Window instances and supports
 * concatenation in horizontal or vertical layouts.
 */
class EnhancedMediaExport_Widget : public QWidget {
    Q_OBJECT

public:
    explicit EnhancedMediaExport_Widget(MediaDisplayCoordinator* coordinator, 
                                       QWidget *parent = nullptr);
    ~EnhancedMediaExport_Widget() override;

    /**
     * @brief Set which displays should be included in export
     */
    void setSelectedDisplays(const std::vector<std::string>& display_ids);

    /**
     * @brief Set the export layout mode
     */
    void setExportLayout(ExportLayout layout);

public slots:
    /**
     * @brief Trigger export with current settings
     */
    void exportMedia();

    /**
     * @brief Preview the export layout
     */
    void previewExport();

private slots:
    void _onLayoutModeChanged();
    void _onDisplaySelectionChanged();
    void _refreshDisplayList();

private:
    Ui::EnhancedMediaExport_Widget *ui;
    MediaDisplayCoordinator* _coordinator;
    std::vector<std::string> _selected_display_ids;
    ExportLayout _export_layout;

    void _setupUI();
    void _updateDisplayCheckboxes();
    
    /**
     * @brief Create a composite image from multiple scenes
     */
    QImage _createCompositeImage(const std::vector<Media_Window*>& scenes);
    
    /**
     * @brief Concatenate images horizontally
     */
    QImage _concatenateHorizontally(const std::vector<QImage>& images);
    
    /**
     * @brief Concatenate images vertically
     */
    QImage _concatenateVertically(const std::vector<QImage>& images);
};

#endif // ENHANCED_MEDIA_EXPORT_WIDGET_HPP
