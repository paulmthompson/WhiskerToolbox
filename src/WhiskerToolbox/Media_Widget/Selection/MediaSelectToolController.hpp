#ifndef MEDIA_SELECT_TOOL_CONTROLLER_HPP
#define MEDIA_SELECT_TOOL_CONTROLLER_HPP

/**
 * @file MediaSelectToolController.hpp
 * @brief Orchestrates unified Select-tool canvas selection and properties sync
 */

#include <QObject>

class Media_Window;
class MediaWidgetState;

/**
 * @brief Enables unified selection on the canvas and syncs hits to MediaWidgetState
 *
 * Active when the Media Viewer Select tool is selected in the left tool strip.
 */
class MediaSelectToolController : public QObject {
    Q_OBJECT

public:
    explicit MediaSelectToolController(QObject * parent = nullptr);

    /**
     * @brief Set the canvas scene used for selection
     * @param window Non-owning Media_Window pointer
     */
    void setMediaWindow(Media_Window * window);

    /**
     * @brief Set shared widget state for preference reads and key sync
     * @param state Non-owning MediaWidgetState pointer
     */
    void setState(MediaWidgetState * state);

    /**
     * @brief Enable or disable unified Select-tool behavior on the canvas
     * @param active True when MediaToolId::Select is active
     */
    void setActive(bool active);

    /**
     * @brief Whether unified Select-tool behavior is active
     * @return True when the Select tool is active
     */
    [[nodiscard]] bool isActive() const { return _active; }

private slots:
    /**
     * @brief Sync canvas selection to MediaWidgetState after user interaction
     */
    void _onCanvasSelectionChanged();

private:
    Media_Window * _window{nullptr};
    MediaWidgetState * _state{nullptr};
    bool _active{false};
};

#endif// MEDIA_SELECT_TOOL_CONTROLLER_HPP
