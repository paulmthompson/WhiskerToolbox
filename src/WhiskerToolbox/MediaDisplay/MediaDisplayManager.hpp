#ifndef MEDIA_DISPLAY_MANAGER_HPP
#define MEDIA_DISPLAY_MANAGER_HPP

#include <QWidget>
#include <memory>
#include <string>

class DataManager;
class Media_Window;
class Media_Widget;

/**
 * @brief Composite class that manages a Media_Widget and its associated Media_Window
 * 
 * This class encapsulates the creation and lifecycle management of a media display unit,
 * ensuring proper coupling between the widget and its graphics scene. This makes it
 * easy to create multiple independent media displays.
 */
class MediaDisplayManager : public QWidget {
    Q_OBJECT

public:
    explicit MediaDisplayManager(std::shared_ptr<DataManager> data_manager, 
                                QWidget* parent = nullptr);
    
    ~MediaDisplayManager() override;

    /**
     * @brief Get the widget component for docking/layout purposes
     */
    Media_Widget* getWidget() const { return _media_widget.get(); }

    /**
     * @brief Get the scene component for export operations
     */
    Media_Window* getScene() const { return _media_scene.get(); }

    /**
     * @brief Update the display with current data
     */
    void updateDisplay();

    /**
     * @brief Load a specific frame
     */
    void loadFrame(int frame_id);

    /**
     * @brief Set feature color for this display
     */
    void setFeatureColor(const std::string& feature, const std::string& hex_color);

    /**
     * @brief Get a unique identifier for this display manager
     */
    std::string getId() const { return _display_id; }

signals:
    /**
     * @brief Emitted when the display content changes (for export coordination)
     */
    void displayContentChanged();

    /**
     * @brief Emitted when this display is selected/focused
     */
    void displaySelected(const std::string& display_id);

private:
    std::unique_ptr<Media_Window> _media_scene;
    std::unique_ptr<Media_Widget> _media_widget;
    std::shared_ptr<DataManager> _data_manager;
    std::string _display_id;

    void _setupConnections();
    void _generateDisplayId();
};

#endif // MEDIA_DISPLAY_MANAGER_HPP
