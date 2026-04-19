/**
 * @file MultiLaneVerticalAxisWidget.hpp
 * @brief Widget for rendering a multi-lane vertical axis with per-lane labels
 *
 * Displays channel/series names alongside each lane in the DataViewer,
 * with horizontal separator lines between lanes. Unlike the single-range
 * VerticalAxisWidget, this widget manages independent lanes that each have
 * their own Y position and extent.
 *
 * Viewport-aware: only renders labels for lanes visible in the current
 * Y viewport (respects y_pan from ViewStateData).
 */

#ifndef MULTI_LANE_VERTICAL_AXIS_WIDGET_HPP
#define MULTI_LANE_VERTICAL_AXIS_WIDGET_HPP

#include <QWidget>

#include <functional>

class MultiLaneVerticalAxisState;
class QPaintEvent;

/**
 * @brief Widget that renders per-lane labels and separators for a stacked layout
 *
 * Shows:
 * - Channel name labels centered vertically in each lane
 * - Horizontal separator lines at lane boundaries
 * - Viewport-aware rendering (culls off-screen lanes)
 *
 * The widget is driven by MultiLaneVerticalAxisState, which receives
 * lane descriptors from the DataViewer after each layout computation.
 */
class MultiLaneVerticalAxisWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Function type that returns the current vertical pan offset
     *
     * The pan offset is in NDC units (matching ViewStateData::y_pan).
     * Used to keep labels aligned with the OpenGL rendering surface.
     */
    using PanGetter = std::function<float()>;

    /**
     * @brief Construct a MultiLaneVerticalAxisWidget
     * @param state State object providing lane descriptors (non-owning)
     * @param parent Parent widget
     */
    explicit MultiLaneVerticalAxisWidget(MultiLaneVerticalAxisState * state,
                                         QWidget * parent = nullptr);

    ~MultiLaneVerticalAxisWidget() override = default;

    /**
     * @brief Update the state pointer (e.g., when DataViewerState changes)
     *
     * Disconnects from the previous state and reconnects to the new one.
     *
     * @param state New state object (non-owning)
     */
    void setState(MultiLaneVerticalAxisState * state);

    /**
     * @brief Set the function to get the current vertical pan offset
     * @param getter Function returning y_pan in NDC units
     */
    void setPanGetter(PanGetter getter);

    /**
     * @brief Set the visible Y range in NDC
     *
     * Determines which lanes are considered on-screen for culling.
     * Defaults to (-1, 1).
     *
     * @param y_min Minimum Y in NDC
     * @param y_max Maximum Y in NDC
     */
    void setYRange(float y_min, float y_max);

    /**
     * @brief Suggested size for the axis widget
     * @return QSize with fixed width of 50px
     */
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent * event) override;

private:
    MultiLaneVerticalAxisState * _state{nullptr};

    PanGetter _pan_getter;
    float _y_min{-1.0f};
    float _y_max{1.0f};

    static constexpr int kAxisWidth = 50;

    /**
     * @brief Convert NDC Y value to pixel Y position
     *
     * Maps from normalized device coordinates (y_min at bottom, y_max at top)
     * to widget pixel coordinates (0 at top, height at bottom), incorporating
     * the current vertical pan offset.
     *
     * @param ndc_y Y value in NDC
     * @return Pixel Y position
     */
    [[nodiscard]] int ndcToPixelY(float ndc_y) const;

    /**
     * @brief Check if a lane is visible in the current viewport
     * @param center Lane center in NDC
     * @param extent Lane height in NDC
     * @return true if any part of the lane is within the viewport
     */
    [[nodiscard]] bool isLaneVisible(float center, float extent) const;
};

#endif// MULTI_LANE_VERTICAL_AXIS_WIDGET_HPP
