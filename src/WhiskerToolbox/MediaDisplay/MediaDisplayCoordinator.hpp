#ifndef MEDIA_DISPLAY_COORDINATOR_HPP
#define MEDIA_DISPLAY_COORDINATOR_HPP

#include <QObject>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

class MediaDisplayManager;
class DataManager;
class Media_Window;

/**
 * @brief Mediator class that coordinates multiple media displays and provides
 * centralized access for operations like export that need to work across displays
 * 
 * This replaces the pattern of MainWindow directly managing Media_Window pointers
 * and instead provides a clean interface for multi-display operations.
 */
class MediaDisplayCoordinator : public QObject {
    Q_OBJECT

public:
    explicit MediaDisplayCoordinator(std::shared_ptr<DataManager> data_manager, 
                                   QObject* parent = nullptr);
    
    ~MediaDisplayCoordinator() override;

    /**
     * @brief Create a new media display and return its manager
     * @return Pointer to the new MediaDisplayManager
     */
    MediaDisplayManager* createMediaDisplay();

    /**
     * @brief Remove a media display by its ID
     */
    void removeMediaDisplay(const std::string& display_id);

    /**
     * @brief Get all active media displays
     */
    std::vector<MediaDisplayManager*> getActiveDisplays() const;

    /**
     * @brief Get a specific display by ID
     */
    MediaDisplayManager* getDisplay(const std::string& display_id) const;

    /**
     * @brief Get the currently selected/active display
     */
    MediaDisplayManager* getActiveDisplay() const;

    /**
     * @brief Get all scenes for export operations (WYSIWYG functionality)
     */
    std::vector<Media_Window*> getAllScenesForExport() const;

    /**
     * @brief Get scenes from selected displays for export
     */
    std::vector<Media_Window*> getSelectedScenesForExport(const std::vector<std::string>& display_ids) const;

    /**
     * @brief Synchronize frame across all displays
     */
    void synchronizeFrame(int frame_id);

    /**
     * @brief Apply feature color change across all displays
     */
    void synchronizeFeatureColor(const std::string& feature, const std::string& hex_color);

signals:
    /**
     * @brief Emitted when a new display is created
     */
    void displayCreated(const std::string& display_id);

    /**
     * @brief Emitted when a display is removed
     */
    void displayRemoved(const std::string& display_id);

    /**
     * @brief Emitted when the active display changes
     */
    void activeDisplayChanged(const std::string& display_id);

private slots:
    void _onDisplaySelected(const std::string& display_id);
    void _onDisplayContentChanged();

private:
    std::shared_ptr<DataManager> _data_manager;
    std::unordered_map<std::string, std::unique_ptr<MediaDisplayManager>> _displays;
    std::string _active_display_id;

    void _setupDisplayConnections(MediaDisplayManager* display);
};

#endif // MEDIA_DISPLAY_COORDINATOR_HPP
