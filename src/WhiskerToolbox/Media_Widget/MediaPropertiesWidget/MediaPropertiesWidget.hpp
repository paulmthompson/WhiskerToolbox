#ifndef MEDIA_PROPERTIES_WIDGET_HPP
#define MEDIA_PROPERTIES_WIDGET_HPP

/**
 * @file MediaPropertiesWidget.hpp
 * @brief Properties panel for the Media Widget
 * 
 * MediaPropertiesWidget is the properties/inspector panel for Media_Widget.
 * It displays controls for managing displayed features and their options.
 * 
 * ## Architecture
 * 
 * The Media Widget follows a View + Properties split:
 * - **Media_Widget** (View): Contains the graphics view and media canvas
 * - **MediaPropertiesWidget** (Properties): Contains feature table, text overlays,
 *   and per-data-type editing widgets
 * 
 * Both widgets share the same MediaWidgetState for coordination.
 * 
 * ## Components
 * 
 * Currently migrated components:
 * - Feature_Table_Widget: Feature selection and visibility toggles
 * - MediaText_Widget: Text overlays (in collapsible section)
 * - QStackedWidget: Per-data-type editing widgets
 *   - MediaPoint_Widget
 *   - MediaLine_Widget
 *   - MediaMask_Widget
 *   - MediaInterval_Widget
 *   - MediaTensor_Widget
 *   - MediaProcessing_Widget
 * 
 * @see Media_Widget for the view component
 * @see MediaWidgetState for shared state
 * @see MediaWidgetRegistration for factory registration
 */

#include <QWidget>

#include <memory>

class DataManager;
class MediaWidgetState;
class Media_Window;
class MediaProcessing_Widget;
class MediaText_Widget;
class Section;

namespace Ui {
class MediaPropertiesWidget;
}

/**
 * @brief Properties panel for Media Widget
 * 
 * Displays feature table, text overlays, and data-type specific controls.
 * Shares state with Media_Widget (view) via MediaWidgetState.
 */
class MediaPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a MediaPropertiesWidget
     * 
     * @param state Shared state with the view widget
     * @param data_manager DataManager for feature queries
     * @param media_window Media_Window for drawing coordination (may be nullptr initially)
     * @param parent Parent widget
     */
    explicit MediaPropertiesWidget(std::shared_ptr<MediaWidgetState> state,
                                    std::shared_ptr<DataManager> data_manager,
                                    Media_Window * media_window = nullptr,
                                    QWidget * parent = nullptr);

    ~MediaPropertiesWidget() override;

    /**
     * @brief Set the Media_Window reference for drawing coordination
     * 
     * Called after construction when the view creates its Media_Window.
     * This allows the properties panel to coordinate with the canvas.
     * 
     * @param media_window The Media_Window from the view widget
     */
    void setMediaWindow(Media_Window * media_window);

    /**
     * @brief Get the current Media_Window reference
     * @return Pointer to Media_Window or nullptr if not set
     */
    [[nodiscard]] Media_Window * getMediaWindow() const { return _media_window; }

private:
    Ui::MediaPropertiesWidget * ui;
    std::shared_ptr<MediaWidgetState> _state;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _media_window{nullptr};

    // Processing widget reference for colormap options
    MediaProcessing_Widget * _processing_widget{nullptr};

    // Text overlay widgets
    Section * _text_section{nullptr};
    MediaText_Widget * _text_widget{nullptr};

    void _connectStateSignals();
    void _setupFeatureTable();
    void _setupTextOverlays();
    void _createStackedWidgets();
    void _connectTextWidgetToScene();

private slots:
    void _featureSelected(QString const & feature);
    void _addFeatureToDisplay(QString const & feature, bool enabled);

signals:
    /**
     * @brief Emitted when a feature is enabled or disabled
     * 
     * Media_Widget listens to this to update the canvas display.
     */
    void featureEnabledChanged(QString const & feature, bool enabled);

    /**
     * @brief Emitted when a feature is selected in the table
     * 
     * Media_Widget may use this to synchronize selection.
     */
    void featureSelected(QString const & feature);
};

#endif // MEDIA_PROPERTIES_WIDGET_HPP
