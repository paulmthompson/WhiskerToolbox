#ifndef WHISKER_TOOLBOX_LINE_DRAW_SELECTION_WIDGET_HPP
#define WHISKER_TOOLBOX_LINE_DRAW_SELECTION_WIDGET_HPP

#include <QWidget>

#include "CoreGeometry/points.hpp"

class QPushButton;
class QLabel;
class QVBoxLayout;

namespace Ui {
class LineDrawSelectionWidget;
}

namespace line_widget {

/**
 * @brief Widget for the "Draw Line" selection mode
 *
 * This widget provides UI for drawing a line on the current frame only.
 * The user can:
 * - Draw a line by clicking points
 * - Complete the line drawing
 * - Apply the line to the current frame
 */
class LineDrawSelectionWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent The parent widget
     */
    explicit LineDrawSelectionWidget(QWidget* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~LineDrawSelectionWidget();

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
     * @brief Signal emitted when the line should be applied to current frame
     */
    void applyToCurrentFrame();

    /**
     * @brief Signal emitted when line points are updated
     */
    void linePointsUpdated();

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
     * @brief Handle the apply to current frame button click
     */
    void _onApplyToCurrentFrameClicked();

    /**
     * @brief Handle the clear line button click
     */
    void _onClearLineClicked();

private:
    Ui::LineDrawSelectionWidget* ui;

    bool _is_drawing_active = false;
    std::vector<Point2D<float>> _current_line_points;

    void _setupUI();
    void _connectSignals();
    void _updateUI();
};

} // namespace line_widget

#endif // WHISKER_TOOLBOX_LINE_DRAW_SELECTION_WIDGET_HPP
