#ifndef MEDIAMASK_WIDGET_HPP
#define MEDIAMASK_WIDGET_HPP

#include "CoreGeometry/masks.hpp"
#include "ImageProcessing/ProcessingOptions.hpp"
#include "Media_Widget/DisplayOptions/CoordinateTypes.hpp"

#include <QMap>
#include <QWidget>

#include <memory>
#include <string>
#include <unordered_map>

namespace Ui {
class MediaMask_Widget;
}

namespace mask_widget {
class MaskNoneSelectionWidget;
class MaskBrushSelectionWidget;
}// namespace mask_widget

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

protected:
    void showEvent(QShowEvent * event) override;
    void hideEvent(QHideEvent * event) override;
    void resizeEvent(QResizeEvent * event) override;

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
    mask_widget::MaskNoneSelectionWidget * _noneSelectionWidget{nullptr};
    mask_widget::MaskBrushSelectionWidget * _brushSelectionWidget{nullptr};

    // Dilation widget and section
    MaskDilationWidget * _dilation_widget{nullptr};
    Section * _dilation_section{nullptr};

    QMap<QString, Selection_Mode> _selection_modes;
    Selection_Mode _selection_mode{Selection_Mode::None};

    // Preview state tracking
    std::unordered_map<std::string, std::vector<Mask2D>> _original_mask_data;
    bool _preview_active{false};

    // Brush drag state
    bool _is_dragging{false};
    bool _is_adding_mode{true};    // true for add, false for remove
    bool _debug_performance{false};// Debug flag for performance-related output
    bool _allow_empty_mask{false}; // Whether to preserve empty masks during brush removal

    // Stroke tracking for erase behavior
    int _stroke_original_primary_size{0};

    void _setupSelectionModePages();
    void _setupDilationWidget();
    void _applyMaskDilation(MaskDilationOptions const & options);
    void _applyDilationPermanently();
    void _storeOriginalMaskData();
    void _restoreOriginalMaskData();

    // Brush functionality
    void _addToMask(CanvasCoordinates const & canvas_coords);
    void _removeFromMask(CanvasCoordinates const & canvas_coords);

private slots:
    void _setMaskAlpha(int alpha);
    void _setMaskColor(QString const & hex_color);
    void _toggleShowBoundingBox(bool checked);
    void _toggleShowOutline(bool checked);
    void _toggleUseAsTransparency(bool checked);
    void _toggleSelectionMode(QString text);
    void _clickedInVideo(CanvasCoordinates const & canvas_coords);
    void _rightClickedInVideo(CanvasCoordinates const & canvas_coords);
    void _mouseMoveInVideo(CanvasCoordinates const & canvas_coords);
    void _mouseReleased();
    void _setBrushSize(int size);
    void _toggleShowHoverCircle(bool checked);
    void _onDilationOptionsChanged(MaskDilationOptions const & options);
    void _onDilationApplyRequested();
    void _onAllowEmptyMaskChanged(bool allow);
};


#endif// MEDIAMASK_WIDGET_HPP
