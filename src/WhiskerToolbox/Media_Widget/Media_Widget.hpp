#ifndef MEDIA_WIDGET_HPP
#define MEDIA_WIDGET_HPP

#include <QWidget>

class DataManager;
class MainWindow;
class Media_Window;

namespace Ui {
class Media_Widget;
}

class Media_Widget : public QWidget {
    Q_OBJECT
public:
    explicit Media_Widget(QWidget * parent = nullptr);

    ~Media_Widget() override;

    void setDataManager(std::shared_ptr<DataManager> data_manager);
    void setScene(Media_Window * scene) { _scene = scene; };
    void setMainWindow(MainWindow * mainwindow) { _main_window = mainwindow; };

    void updateMedia();

    void setFeatureColor(std::string const & feature, std::string const & hex_color);

protected:
    void resizeEvent(QResizeEvent * event) override;

private:
    Ui::Media_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene = nullptr;

    MainWindow * _main_window = nullptr;


private slots:
    void _updateCanvasSize();
    void _addFeatureToDisplay(QString const & feature, bool enabled);
    void _featureSelected(QString const & feature);
signals:
};

#endif// MEDIA_WIDGET_HPP
