/**
 * @file MediaDebugPanel.hpp
 * @brief Developer diagnostics panel for the Media Widget
 *
 * Provides real-time visibility into Media_Window scene element counts.
 * Gated behind MediaWidgetState::developerMode().
 */

#ifndef MEDIA_DEBUG_PANEL_HPP
#define MEDIA_DEBUG_PANEL_HPP

#include <QWidget>

#include <memory>

class MediaWidgetState;
class Media_Window;
class QLabel;
class Section;

/**
 * @brief Developer diagnostics panel showing scene element counts
 *
 * Organized into a collapsible Section:
 *  - Scene Elements (counts of each internal QGraphicsScene vector)
 */
class MediaDebugPanel : public QWidget {
    Q_OBJECT
public:
    /**
     * @brief Construct the debug panel
     * @param state Shared media widget state
     * @param media_window Non-owning pointer to the media canvas
     * @param parent Parent widget
     */
    explicit MediaDebugPanel(std::shared_ptr<MediaWidgetState> state,
                             Media_Window * media_window,
                             QWidget * parent = nullptr);

    ~MediaDebugPanel() override = default;

    /**
     * @brief Update the Media_Window reference and reconnect refresh signals
     * @param media_window Non-owning pointer to the media canvas
     */
    void setMediaWindow(Media_Window * media_window);

public slots:
    /// Refresh all sections from current diagnostics
    void refresh();

private:
    std::shared_ptr<MediaWidgetState> _state;
    Media_Window * _media_window = nullptr;///< Non-owning

    Section * _scene_elements_section = nullptr;

    QLabel * _line_paths_label = nullptr;
    QLabel * _masks_label = nullptr;
    QLabel * _mask_bounding_boxes_label = nullptr;
    QLabel * _mask_outlines_label = nullptr;
    QLabel * _points_label = nullptr;
    QLabel * _intervals_label = nullptr;
    QLabel * _tensors_label = nullptr;
    QLabel * _text_items_label = nullptr;
    QLabel * _total_scene_items_label = nullptr;

    void _buildUI();
    void _connectSignals();
    void _refreshSceneElements();
};

#endif// MEDIA_DEBUG_PANEL_HPP
