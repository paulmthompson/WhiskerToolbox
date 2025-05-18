#ifndef MEDIALINE_WIDGET_HPP
#define MEDIALINE_WIDGET_HPP


#include "DataManager/Lines/lines.hpp"

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
    
    // Method to handle time changes
    void LoadFrame(int frame_id);

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
    
    // Smoothing modes for the Add selection mode
    enum class Smoothing_Mode {
        SimpleSmooth,
        PolynomialFit
    };
    
    QMap<QString, Selection_Mode> _selection_modes;
    Selection_Mode _selection_mode {Selection_Mode::None};
    Smoothing_Mode _smoothing_mode {Smoothing_Mode::SimpleSmooth};
    int _polynomial_order {3}; // Default polynomial order
    int _current_line_index {0}; // Track which line is currently selected
    
    void _setupSelectionModePages();
    void _addPointToLine(float x_media, float y_media, int current_time);
    void _applyPolynomialFit(Line2D& line, int order);
    
private slots:
    void _clickedInVideo(qreal x, qreal y);
    void _toggleSelectionMode(QString text);
    void _setSmoothingMode(int index);
    void _setPolynomialOrder(int order);
    void _setLineAlpha(int alpha);
    void _setLineColor(const QString& hex_color);
    void _toggleShowPoints(bool checked);
    void _lineSelectionChanged(int index);
    //void _clearCurrentLine();
};

#endif// MEDIALINE_WIDGET_HPP
