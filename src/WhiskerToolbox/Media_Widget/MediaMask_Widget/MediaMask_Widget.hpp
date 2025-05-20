#ifndef MEDIAMASK_WIDGET_HPP
#define MEDIAMASK_WIDGET_HPP

#include <QWidget>
#include <QMap>

#include <string>
#include <memory>

namespace Ui {
class MediaMask_Widget;
}

namespace mask_widget {
class MaskNoneSelectionWidget;
}

class DataManager;
class Media_Window;

class MediaMask_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaMask_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent = nullptr);
    ~MediaMask_Widget() override;

    void setActiveKey(std::string const & key);
    void showEvent(QShowEvent * event) override;
    void hideEvent(QHideEvent * event) override;

private:
    Ui::MediaMask_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    std::string _active_key;
    
    // Selection mode enum
    enum class Selection_Mode {
        None,
        Draw,
        Erase
    };
    
    // Selection widget pointers
    mask_widget::MaskNoneSelectionWidget* _noneSelectionWidget {nullptr};
    
    QMap<QString, Selection_Mode> _selection_modes;
    Selection_Mode _selection_mode {Selection_Mode::None};
    
    void _setupSelectionModePages();

private slots:
    void _setMaskAlpha(int alpha);
    void _setMaskColor(const QString& hex_color);
    void _toggleSelectionMode(QString text);
    void _clickedInVideo(qreal x, qreal y);
};


#endif// MEDIAMASK_WIDGET_HPP
