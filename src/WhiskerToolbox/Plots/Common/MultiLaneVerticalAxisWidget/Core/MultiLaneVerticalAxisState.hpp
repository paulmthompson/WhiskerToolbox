/**
 * @file MultiLaneVerticalAxisState.hpp
 * @brief QObject state wrapper for the multi-lane vertical axis widget
 *
 * Wraps MultiLaneVerticalAxisStateData with Qt signals for change notification.
 * The DataViewer pushes lane descriptors after each layout computation;
 * the widget repaints in response.
 */

#ifndef MULTI_LANE_VERTICAL_AXIS_STATE_HPP
#define MULTI_LANE_VERTICAL_AXIS_STATE_HPP

#include "MultiLaneVerticalAxisStateData.hpp"

#include <QObject>

#include <vector>

/**
 * @brief State class for the multi-lane vertical axis widget
 *
 * Owns the lane descriptor list and emits signals when lanes change.
 * The axis widget connects to these signals to trigger repaint.
 *
 * Thread safety: NOT thread-safe — access from the GUI thread only.
 */
class MultiLaneVerticalAxisState : public QObject {
    Q_OBJECT

public:
    explicit MultiLaneVerticalAxisState(QObject * parent = nullptr);
    ~MultiLaneVerticalAxisState() override = default;

    // === Lane Descriptors ===

    /**
     * @brief Replace the full set of lane descriptors
     *
     * Called by DataViewer after each layout computation.
     * Emits lanesChanged().
     *
     * @param lanes New lane descriptors
     */
    void setLanes(std::vector<LaneAxisDescriptor> lanes);

    /**
     * @brief Get the current lane descriptors
     * @return Const reference to the lane descriptor vector
     */
    [[nodiscard]] std::vector<LaneAxisDescriptor> const & lanes() const { return _data.lanes; }

    // === Visibility Toggles ===

    /**
     * @brief Set whether lane labels are shown
     * @param show true to show labels
     */
    void setShowLabels(bool show);

    /**
     * @brief Check if labels are shown
     */
    [[nodiscard]] bool showLabels() const { return _data.show_labels; }

    /**
     * @brief Set whether lane separators are shown
     * @param show true to show separators
     */
    void setShowSeparators(bool show);

    /**
     * @brief Check if separators are shown
     */
    [[nodiscard]] bool showSeparators() const { return _data.show_separators; }

    // === Data Access ===

    /**
     * @brief Get const reference to underlying state data
     */
    [[nodiscard]] MultiLaneVerticalAxisStateData const & data() const { return _data; }

signals:
    /**
     * @brief Emitted when the lane descriptor list changes
     */
    void lanesChanged();

    /**
     * @brief Emitted when visibility settings change (labels or separators)
     */
    void visibilityChanged();

private:
    MultiLaneVerticalAxisStateData _data;
};

#endif// MULTI_LANE_VERTICAL_AXIS_STATE_HPP
