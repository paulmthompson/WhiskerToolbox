#ifndef MEDIA_WIDGET_HPP
#define MEDIA_WIDGET_HPP

#include <QWidget>
#include <memory>

class DataManager;
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
    
    /**
     * @brief Get the Media_Window owned by this widget
     * @return Pointer to the Media_Window
     */
    Media_Window* getMediaWindow() const { return _scene.get(); }

    void updateMedia();

    void setFeatureColor(std::string const & feature, std::string const & hex_color);

    // Method to handle time changes and propagate them
    void LoadFrame(int frame_id);

protected:
    void resizeEvent(QResizeEvent * event) override;

private:
    Ui::Media_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    std::unique_ptr<Media_Window> _scene;
    std::map<std::string, std::vector<int>> _callback_ids;

    // Text overlay widgets
    Section * _text_section = nullptr;
    MediaText_Widget * _text_widget = nullptr;

    void _createOptions();
    void _createMediaWindow();
    void _connectTextWidgetToScene();
    
    // Media management helpers
    void _disableAllMediaExcept(std::string const & enabled_key);
    void _selectAlternativeMedia(std::string const & disabled_key);
    void _setupDefaultMediaState();

private slots:
    void _updateCanvasSize();
    void _addFeatureToDisplay(QString const & feature, bool enabled);
    void _featureSelected(QString const & feature);
signals:
};

#endif// MEDIA_WIDGET_HPP
