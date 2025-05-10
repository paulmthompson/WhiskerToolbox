#ifndef MEDIALINE_WIDGET_HPP
#define MEDIALINE_WIDGET_HPP


#include <QMap>
#include <QWidget>

#include <memory>
#include <string>

namespace Ui {
class MediaLine_Widget;
}

class DataManager;
class Media_Window;

class MediaLine_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaLine_Widget(std::shared_ptr<DataManager> data_manager, Media_Window* scene, QWidget* parent = nullptr);
    ~MediaLine_Widget() override;
    void showEvent(QShowEvent * event) override;
    void hideEvent(QHideEvent * event) override;

    void setActiveKey(std::string const& key);

private:
    Ui::MediaLine_Widget* ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window* _scene;
    std::string _active_key;
    enum class Selection_Mode {
        None,
        Add,
        Erase
    };
    QMap<QString, std::pair<Selection_Mode, QString>> _selection_modes;
    Selection_Mode _selection_mode {Selection_Mode::None};
private slots:
    void _clickedInVideo(qreal x, qreal y);
    void _toggleSelectionMode(QString text);
    void _setLineAlpha(int alpha);
    void _setLineColor(const QString& hex_color);
    //void _clearCurrentLine();
};

#endif// MEDIALINE_WIDGET_HPP
