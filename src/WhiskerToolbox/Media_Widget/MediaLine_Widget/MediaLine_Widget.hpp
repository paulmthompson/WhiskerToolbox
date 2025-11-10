#ifndef MEDIALINE_WIDGET_HPP
#define MEDIALINE_WIDGET_HPP


#include "CoreGeometry/lines.hpp"
#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <QMap>
#include <QWidget>

#include <memory>
#include <optional>
#include <opencv2/opencv.hpp>
#include <string>

namespace Ui {
class MediaLine_Widget;
}

namespace line_widget {
class LineNoneSelectionWidget;
class LineAddSelectionWidget;
class LineEraseSelectionWidget;
class LineSelectSelectionWidget;
class LineDrawAllFramesSelectionWidget;
}// namespace line_widget

class DataManager;
class Media_Window;

class MediaLine_Widget : public QWidget {
    Q_OBJECT
public:
    explicit MediaLine_Widget(std::shared_ptr<DataManager> data_manager, Media_Window * scene, QWidget * parent = nullptr);
    ~MediaLine_Widget() override;
    void showEvent(QShowEvent * event) override;
    void hideEvent(QHideEvent * event) override;

    void setActiveKey(std::string const & key);

    // Method to handle time changes
    void LoadFrame(int frame_id);

private:
    Ui::MediaLine_Widget * ui;
    std::shared_ptr<DataManager> _data_manager;
    Media_Window * _scene;
    std::string _active_key;
    enum class Selection_Mode {
        None,
        Add,
        Erase,
        Select,
        DrawAllFrames
    };

    enum class Smoothing_Mode {
        SimpleSmooth,
        PolynomialFit
    };

    line_widget::LineNoneSelectionWidget * _noneSelectionWidget{nullptr};
    line_widget::LineAddSelectionWidget * _addSelectionWidget{nullptr};
    line_widget::LineEraseSelectionWidget * _eraseSelectionWidget{nullptr};
    line_widget::LineSelectSelectionWidget * _selectSelectionWidget{nullptr};
    line_widget::LineDrawAllFramesSelectionWidget * _drawAllFramesSelectionWidget{nullptr};

    QMap<QString, Selection_Mode> _selection_modes;
    Selection_Mode _selection_mode{Selection_Mode::None};
    Smoothing_Mode _smoothing_mode{Smoothing_Mode::SimpleSmooth};
    int _polynomial_order{3};              // Default polynomial order
    int _current_line_index{0};            // Track which line is currently selected
    int _selected_line_index{-1};          // Track which line is selected for operations (-1 = none)
    float _line_selection_threshold{15.0f};// Pixel threshold for line selection

    // Edge detection parameters
    bool _edge_snapping_enabled{false};
    int _edge_threshold{100};   // Default Canny edge detection threshold
    int _edge_search_radius{20};// Default search radius in pixels
    cv::Mat _current_edges;     // Cached edge detection results
    cv::Mat _current_frame;     // Cached current frame

    // Flag to prevent infinite loops during percentage updates
    bool _is_updating_percentages{false};

    void _setupSelectionModePages();
    void _addPointToLine(float x_media, float y_media, TimeFrameIndex current_time);
    void _erasePointsFromLine(float x_media, float y_media, TimeFrameIndex current_time);
    void _applyPolynomialFit(Line2D & line, int order);

    void _detectEdges();
    std::pair<float, float> _findNearestEdge(float x, float y);

    // Line selection methods
    std::optional<EntityId> _findNearestLine(float x, float y);
    void _selectLine(int line_index);
    void _clearLineSelection();

    // Helper function for calculating distance from point to line segment
    float _calculateDistanceToLineSegment(Point2D<float> const & point,
                                          Point2D<float> const & line_start,
                                          Point2D<float> const & line_end);

    // Context menu and operations
    void _showLineContextMenu(QPoint const & position);
    void _moveLineToTarget(std::string const & target_key);
    void _copyLineToTarget(std::string const & target_key);
    std::vector<std::string> _getAvailableLineDataKeys();

    // Draw across all frames methods
    void _addPointToDrawAllFrames(float x_media, float y_media);
    void _applyLineToAllFrames();
    std::vector<TimeFrameIndex> _getAllFrameTimes();
    void _updateTemporaryLineFromWidget();

private slots:
    void _clickedInVideoWithModifiers(qreal x, qreal y, Qt::KeyboardModifiers modifiers);
    void _rightClickedInVideo(qreal x, qreal y);
    void _mouseMoved(qreal x, qreal y);
    void _toggleSelectionMode(QString text);
    void _setSmoothingMode(int index);
    void _setPolynomialOrder(int order);
    void _setLineAlpha(int alpha);
    void _setLineColor(QString const & hex_color);
    void _setLineThickness(int thickness);
    void _toggleShowPoints(bool checked);
    void _toggleShowPositionMarker(bool checked);
    void _setPositionPercentage(int percentage);
    void _toggleShowSegment(bool checked);
    void _setSegmentStartPercentage(int percentage);
    void _setSegmentEndPercentage(int percentage);
    void _toggleEdgeSnapping(bool checked);
    void _setEdgeThreshold(int threshold);
    void _setEdgeSearchRadius(int radius);
    void _setEraserRadius(int radius);
    void _toggleShowHoverCircle(bool checked);
    
    // Helper method to get selected EntityId from group-based selection system
    std::optional<EntityId> _getSelectedEntityIdFromGroupSystem() const;
};

#endif// MEDIALINE_WIDGET_HPP
