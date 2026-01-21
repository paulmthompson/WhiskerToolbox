#ifndef MEDIA_WIDGET_HPP
#define MEDIA_WIDGET_HPP

#include "EditorState/SelectionContext.hpp"  // For SelectionSource

#include <QWidget>

#include <memory>
#include <set>

class DataManager;
class Media_Window;
class MediaText_Widget;
class MediaProcessing_Widget;
class MediaWidgetState;
class Section;
class EditorRegistry;

namespace Ui {
class Media_Widget;
}

class Media_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Media_Widget(EditorRegistry * editor_registry = nullptr,
                          QWidget * parent = nullptr);

    ~Media_Widget() override;

    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Get the Media_Window owned by this widget
     * @return Pointer to the Media_Window
     */
    [[nodiscard]] Media_Window * getMediaWindow() const { return _scene.get(); }

    /**
     * @brief Get the state object for this widget
     * @return Shared pointer to MediaWidgetState
     */
    [[nodiscard]] std::shared_ptr<MediaWidgetState> getState() const { return _state; }

    void updateMedia();

    void setFeatureColor(std::string const & feature, std::string const & hex_color);

    // Method to handle time changes and propagate them
    void LoadFrame(int frame_id);

    // Zoom API used by MainWindow actions
    void zoomIn();
    void zoomOut();
    void resetZoom();

    /**
     * @brief Restore widget state from the state object
     * 
     * Call this after loading a workspace to apply serialized state
     * (zoom, pan, display options, etc.) to the widget.
     */
    void restoreFromState();

protected:
    void resizeEvent(QResizeEvent * event) override;
    bool eventFilter(QObject * watched, QEvent * event) override;// intercept wheel events for zoom

private:
    Ui::Media_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    EditorRegistry * _editor_registry{nullptr};
    std::unique_ptr<Media_Window> _scene;
    std::map<std::string, std::vector<int>> _callback_ids;

    // Text overlay widgets
    Section * _text_section = nullptr;
    MediaText_Widget * _text_widget = nullptr;

    // Processing widget for colormap options
    MediaProcessing_Widget * _processing_widget = nullptr;

    // Zoom configuration constants (actual zoom value stored in state)
    static constexpr double _zoom_step{1.15};// multiplicative step per wheel/action
    static constexpr double _min_zoom{0.1};
    static constexpr double _max_zoom{20.0};

    // Canvas panning state for shift+drag
    bool _is_panning{false};
    QPoint _last_pan_point;

    // Editor state for workspace serialization and inter-widget communication
    std::shared_ptr<MediaWidgetState> _state;
    SelectionContext * _selection_context{nullptr};

    void _applyZoom(double factor, bool anchor_under_mouse);
    
    // Helper to check if user has applied a non-default zoom
    [[nodiscard]] bool _isUserZoomActive() const;

    void _createOptions();
    void _createMediaWindow();
    void _connectTextWidgetToScene();

    // State synchronization helpers
    void _syncCanvasSizeToState();
    void _syncFeatureEnabledToState(QString const & feature_key, QString const & data_type, bool enabled);
    void _connectStateSignals();


private slots:
    void _updateCanvasSize();
    void _addFeatureToDisplay(QString const & feature, bool enabled);
    void _featureSelected(QString const & feature);
    void _onExternalSelectionChanged(SelectionSource const & source);
    void _onStateZoomChanged(double zoom);
    void _onStatePanChanged(double x, double y);
signals:
};

#endif// MEDIA_WIDGET_HPP
