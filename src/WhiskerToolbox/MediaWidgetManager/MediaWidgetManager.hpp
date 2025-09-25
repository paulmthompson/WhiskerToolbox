#ifndef MEDIA_WIDGET_MANAGER_HPP
#define MEDIA_WIDGET_MANAGER_HPP

#include "Media_Widget/Media_Widget.hpp"

#include <QObject>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class DataManager;
class Media_Window;
class GroupManager;

/**
 * @brief Manager class for multiple Media_Widget instances
 * 
 * This class acts as a mediator/manager for multiple Media_Widget instances,
 * each with their own Media_Window. It provides centralized access to media
 * widgets and handles signal routing between widgets.
 */
class MediaWidgetManager : public QObject {
    Q_OBJECT

public:
    explicit MediaWidgetManager(std::shared_ptr<DataManager> data_manager, QObject* parent = nullptr);
    ~MediaWidgetManager() override = default;

    /**
     * @brief Set the GroupManager for group-aware plotting
     * @param group_manager Pointer to the GroupManager instance
     */
    void setGroupManager(GroupManager* group_manager);

    /**
     * @brief Create a new Media_Widget with its own Media_Window
     * @param id Unique identifier for the media widget
     * @param parent Parent widget for the Media_Widget
     * @return Pointer to the created Media_Widget, or nullptr if id already exists
     */
    Media_Widget* createMediaWidget(const std::string& id, QWidget* parent = nullptr);

    /**
     * @brief Remove a Media_Widget by id
     * @param id Identifier of the media widget to remove
     * @return True if widget was found and removed, false otherwise
     */
    bool removeMediaWidget(const std::string& id);

    /**
     * @brief Get a Media_Widget by id
     * @param id Identifier of the media widget
     * @return Pointer to the Media_Widget, or nullptr if not found
     */
    Media_Widget* getMediaWidget(const std::string& id) const;

    /**
     * @brief Get the Media_Window associated with a Media_Widget
     * @param id Identifier of the media widget
     * @return Pointer to the Media_Window, or nullptr if not found
     */
    Media_Window* getMediaWindow(const std::string& id) const;

    /**
     * @brief Get all media widget identifiers
     * @return Vector of all media widget IDs
     */
    std::vector<std::string> getMediaWidgetIds() const;

    /**
     * @brief Set feature color for all media widgets
     * @param feature Feature name
     * @param hex_color Color in hex format
     */
    void setFeatureColorForAll(const std::string& feature, const std::string& hex_color);

    /**
     * @brief Set feature color for a specific media widget
     * @param widget_id Media widget identifier
     * @param feature Feature name
     * @param hex_color Color in hex format
     */
    void setFeatureColor(const std::string& widget_id, const std::string& feature, const std::string& hex_color);

    /**
     * @brief Load frame for all media widgets
     * @param frame_id Frame to load
     */
    void loadFrameForAll(int frame_id);

    /**
     * @brief Load frame for a specific media widget
     * @param widget_id Media widget identifier
     * @param frame_id Frame to load
     */
    void loadFrame(const std::string& widget_id, int frame_id);

    /**
     * @brief Update media for all widgets
     */
    void updateMediaForAll();

    /**
     * @brief Update canvas for all media widgets (useful for group changes)
     */
    void updateCanvasForAll();

signals:
    /**
     * @brief Emitted when a media widget is created
     * @param id Media widget identifier
     * @param widget Pointer to the created widget
     */
    void mediaWidgetCreated(const std::string& id, Media_Widget* widget);

    /**
     * @brief Emitted when a media widget is removed
     * @param id Media widget identifier
     */
    void mediaWidgetRemoved(const std::string& id);

    /**
     * @brief Emitted when a frame is loaded on any media widget
     * @param widget_id Media widget identifier
     * @param frame_id Frame that was loaded
     */
    void frameLoaded(const std::string& widget_id, int frame_id);

private:
    std::shared_ptr<DataManager> _data_manager;
    std::unordered_map<std::string, std::unique_ptr<Media_Widget>> _media_widgets;
    GroupManager* _group_manager{nullptr};

    void _connectWidgetSignals(const std::string& id, Media_Widget* widget);
};

#endif // MEDIA_WIDGET_MANAGER_HPP
