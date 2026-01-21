#ifndef MEDIA_WIDGET_MANAGER_HPP
#define MEDIA_WIDGET_MANAGER_HPP

#include <QObject>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class DataManager;
class Media_Widget;
class Media_Window;
class GroupManager;
class EditorRegistry;

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
    explicit MediaWidgetManager(std::shared_ptr<DataManager> data_manager,
                                EditorRegistry * editor_registry = nullptr,
                                QObject * parent = nullptr);
    ~MediaWidgetManager();

    /**
     * @brief Set the GroupManager for group-aware plotting
     * @param group_manager Pointer to the GroupManager instance
     */
    void setGroupManager(GroupManager * group_manager);

    /**
     * @brief Create a new Media_Widget with its own Media_Window
     * @param id Unique identifier for the media widget
     * @param parent Parent widget for the Media_Widget
     * @return Pointer to the created Media_Widget, or nullptr if id already exists
     */
    Media_Widget * createMediaWidget(std::string const & id, QWidget * parent = nullptr);

    /**
     * @brief Remove a Media_Widget by id
     * @param id Identifier of the media widget to remove
     * @return True if widget was found and removed, false otherwise
     */
    bool removeMediaWidget(std::string const & id);

    /**
     * @brief Get a Media_Widget by id
     * @param id Identifier of the media widget
     * @return Pointer to the Media_Widget, or nullptr if not found
     */
    Media_Widget * getMediaWidget(std::string const & id) const;

    /**
     * @brief Get the Media_Window associated with a Media_Widget
     * @param id Identifier of the media widget
     * @return Pointer to the Media_Window, or nullptr if not found
     */
    Media_Window * getMediaWindow(std::string const & id) const;

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
    void setFeatureColorForAll(std::string const & feature, std::string const & hex_color);

    /**
     * @brief Set feature color for a specific media widget
     * @param widget_id Media widget identifier
     * @param feature Feature name
     * @param hex_color Color in hex format
     */
    void setFeatureColor(std::string const & widget_id, std::string const & feature, std::string const & hex_color);

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
    void loadFrame(std::string const & widget_id, int frame_id);

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
    void mediaWidgetCreated(std::string const & id, Media_Widget * widget);

    /**
     * @brief Emitted when a media widget is removed
     * @param id Media widget identifier
     */
    void mediaWidgetRemoved(std::string const & id);

    /**
     * @brief Emitted when a frame is loaded on any media widget
     * @param widget_id Media widget identifier
     * @param frame_id Frame that was loaded
     */
    void frameLoaded(std::string const & widget_id, int frame_id);

private:
    std::shared_ptr<DataManager> _data_manager;
    EditorRegistry * _editor_registry{nullptr};
    std::unordered_map<std::string, std::unique_ptr<Media_Widget>> _media_widgets;
    GroupManager * _group_manager{nullptr};

    void _connectWidgetSignals(std::string const & id, Media_Widget * widget);
};

#endif// MEDIA_WIDGET_MANAGER_HPP
