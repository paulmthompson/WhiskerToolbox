#ifndef MEDIA_WIDGET_HPP
#define MEDIA_WIDGET_HPP

#include <QWidget>

#include <memory>
#include <set>

class DataManager;
class Media_Window;
class MediaText_Widget;
class MediaProcessing_Widget;
class Section;

namespace Ui {
class Media_Widget;
}

class Media_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Media_Widget(QWidget * parent = nullptr);

    ~Media_Widget() override;

    void setDataManager(std::shared_ptr<DataManager> data_manager);

    /**
     * @brief Get the Media_Window owned by this widget
     * @return Pointer to the Media_Window
     */
    [[nodiscard]] Media_Window * getMediaWindow() const { return _scene.get(); }

    void updateMedia();

    void setFeatureColor(std::string const & feature, std::string const & hex_color);

    // Method to handle time changes and propagate them
    void LoadFrame(int frame_id);

    // Zoom API used by MainWindow actions
    void zoomIn();
    void zoomOut();
    void resetZoom();

protected:
    void resizeEvent(QResizeEvent * event) override;
    bool eventFilter(QObject * watched, QEvent * event) override;// intercept wheel events for zoom

private:
    Ui::Media_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<Media_Window> _scene;
    std::map<std::string, std::vector<int>> _callback_ids;

    // Text overlay widgets
    Section * _text_section = nullptr;
    MediaText_Widget * _text_widget = nullptr;

    // Processing widget for colormap options
    MediaProcessing_Widget * _processing_widget = nullptr;

    // Zoom state
    double _current_zoom{1.0};
    bool _user_zoom_active{false};
    double _zoom_step{1.15};// multiplicative step per wheel/action
    double _min_zoom{0.1};
    double _max_zoom{20.0};

    // Canvas panning state for shift+drag
    bool _is_panning{false};
    QPoint _last_pan_point;

    void _applyZoom(double factor, bool anchor_under_mouse);

    void _createOptions();
    void _createMediaWindow();
    void _connectTextWidgetToScene();


private slots:
    void _updateCanvasSize();
    void _addFeatureToDisplay(QString const & feature, bool enabled);
    void _featureSelected(QString const & feature);
signals:
};

#endif// MEDIA_WIDGET_HPP
