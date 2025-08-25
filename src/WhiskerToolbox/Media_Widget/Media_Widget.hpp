#ifndef MEDIA_WIDGET_HPP
#define MEDIA_WIDGET_HPP

#include <QWidget>

class DataManager;
class MainWindow;
class Media_Window;
class MediaText_Widget;
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
    void setScene(Media_Window * scene) {
        _scene = scene;
        _connectTextWidgetToScene();
    };

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
    bool eventFilter(QObject * watched, QEvent * event) override; // intercept wheel events for zoom

private:
    Ui::Media_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene = nullptr;
    std::map<std::string, std::vector<int>> _callback_ids;

    // Text overlay widgets
    Section * _text_section = nullptr;
    MediaText_Widget * _text_widget = nullptr;

    // Zoom state
    double _current_zoom {1.0};
    bool _user_zoom_active {false};
    double _zoom_step {1.15}; // multiplicative step per wheel/action
    double _min_zoom {0.1};
    double _max_zoom {20.0};

    void _applyZoom(double factor, bool anchor_under_mouse);

    void _createOptions();
    void _connectTextWidgetToScene();

private slots:
    void _updateCanvasSize();
    void _addFeatureToDisplay(QString const & feature, bool enabled);
    void _featureSelected(QString const & feature);
signals:
};

#endif// MEDIA_WIDGET_HPP
