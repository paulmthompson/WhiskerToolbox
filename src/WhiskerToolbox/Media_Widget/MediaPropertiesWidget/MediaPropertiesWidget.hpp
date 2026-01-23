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
 * ## Phase 1: Empty Widget
 * 
 * This is the initial empty implementation. Components will be migrated
 * incrementally from Media_Widget's left panel:
 * 
 * 1. Feature_Table_Widget (feature selection and visibility)
 * 2. MediaText_Widget (text overlays)
 * 3. Stacked widgets for data-type specific editing
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

    void _connectStateSignals();
};

#endif // MEDIA_PROPERTIES_WIDGET_HPP
