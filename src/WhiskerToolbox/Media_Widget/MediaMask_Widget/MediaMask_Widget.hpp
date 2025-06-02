#ifndef MEDIAMASK_WIDGET_HPP
#define MEDIAMASK_WIDGET_HPP

#include "DataManager/Points/points.hpp"
#include "DataManager/utils/ProcessingOptions.hpp"

#include <QWidget>
#include <QMap>

#include <string>
#include <memory>
#include <unordered_map>

namespace Ui {
class MediaMask_Widget;
}

namespace mask_widget {
class MaskNoneSelectionWidget;
class MaskBrushSelectionWidget;
}

class DataManager;
class Media_Window;
class MaskDilationWidget;
class Section;

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
        Brush
    };
    
    // Selection widget pointers
    mask_widget::MaskNoneSelectionWidget* _noneSelectionWidget {nullptr};
    mask_widget::MaskBrushSelectionWidget* _brushSelectionWidget {nullptr};
    
    // Dilation widget and section
    MaskDilationWidget* _dilation_widget {nullptr};
    Section* _dilation_section {nullptr};
    
    QMap<QString, Selection_Mode> _selection_modes;
    Selection_Mode _selection_mode {Selection_Mode::None};
    
    // Preview state tracking
    std::unordered_map<std::string, std::vector<std::vector<Point2D<float>>>> _original_mask_data;
    bool _preview_active {false};
    
    void _setupSelectionModePages();
    void _setupDilationWidget();
    void _applyMaskDilation(MaskDilationOptions const& options);
    void _applyDilationPermanently();
    void _storeOriginalMaskData();
    void _restoreOriginalMaskData();

private slots:
    void _setMaskAlpha(int alpha);
    void _setMaskColor(const QString& hex_color);
    void _toggleShowBoundingBox(bool checked);
    void _toggleShowOutline(bool checked);
    void _toggleSelectionMode(QString text);
    void _clickedInVideo(qreal x, qreal y);
    void _rightClickedInVideo(qreal x, qreal y);
    void _setBrushSize(int size);
    void _toggleShowHoverCircle(bool checked);
    void _onDilationOptionsChanged(MaskDilationOptions const& options);
    void _onDilationApplyRequested();
};


#endif// MEDIAMASK_WIDGET_HPP
