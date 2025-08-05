#ifndef WHISKER_TOOLBOX_LINE_DRAW_ALL_FRAMES_SELECTION_WIDGET_HPP
#define WHISKER_TOOLBOX_LINE_DRAW_ALL_FRAMES_SELECTION_WIDGET_HPP

#include <QWidget>

#include "CoreGeometry/points.hpp"

class QPushButton;
class QLabel;
class QVBoxLayout;

namespace Ui {
class LineDrawAllFramesSelectionWidget;
}

namespace line_widget {

/**
 * @brief Widget for the "Draw Across All Frames" selection mode
 * 
 * This widget provides UI for drawing a line that will be applied to all frames
 * in the media. The user can:
 * - Draw a line by clicking points
 * - Complete the line drawing
 * - Apply the line to all frames
 */
class LineDrawAllFramesSelectionWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent The parent widget
     */
    explicit LineDrawAllFramesSelectionWidget(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~LineDrawAllFramesSelectionWidget();

    /**
     * @brief Check if line drawing is currently active
     */
    bool isDrawingActive() const;

    /**
     * @brief Get the current line points
     */
    std::vector<Point2D<float>> getCurrentLinePoints() const;

    /**
     * @brief Clear the current line points
     */
    void clearLinePoints();

    /**
     * @brief Add a point to the current line
     * @param point The point to add
     */
    void addPoint(const Point2D<float>& point);

signals:
    /**
     * @brief Signal emitted when line drawing is started
     */
    void lineDrawingStarted();
    
    /**
     * @brief Signal emitted when line drawing is completed
     */
    void lineDrawingCompleted();
    
    /**
     * @brief Signal emitted when the line should be applied to all frames
     */
    void applyToAllFrames();

private slots:
    /**
     * @brief Handle the start drawing button click
     */
    void _onStartDrawingClicked();
    
    /**
     * @brief Handle the complete drawing button click
     */
    void _onCompleteDrawingClicked();
    
    /**
     * @brief Handle the apply to all frames button click
     */
    void _onApplyToAllFramesClicked();
    
    /**
     * @brief Handle the clear line button click
     */
    void _onClearLineClicked();

private:
    Ui::LineDrawAllFramesSelectionWidget* ui;
    
    bool _is_drawing_active = false;
    std::vector<Point2D<float>> _current_line_points;
    
    void _setupUI();
    void _connectSignals();
    void _updateUI();
};

} // namespace line_widget

#endif // WHISKER_TOOLBOX_LINE_DRAW_ALL_FRAMES_SELECTION_WIDGET_HPP 