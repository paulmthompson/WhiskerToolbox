#ifndef ONION_SKIN_VIEW_PROPERTIES_WIDGET_HPP
#define ONION_SKIN_VIEW_PROPERTIES_WIDGET_HPP

/**
 * @file OnionSkinViewPropertiesWidget.hpp
 * @brief Properties panel for the Onion Skin View Widget
 *
 * Provides controls for:
 * - Adding/removing point, line, and mask data keys from DataManager
 * - Temporal window size (frames behind / ahead)
 * - Alpha curve type (Linear, Exponential, Gaussian) and min/max alpha
 * - Point size and line width rendering controls
 * - Current-frame highlight toggle
 * - Axis range controls via HorizontalAxisRangeControls and
 *   VerticalAxisRangeControls in collapsible sections
 *
 * @see OnionSkinViewWidget, OnionSkinViewState,
 * OnionSkinViewWidgetRegistration
 */

#include "Core/OnionSkinViewState.hpp"

#include <QWidget>

#include <memory>

class DataManager;
class HorizontalAxisRangeControls;
class OnionSkinViewWidget;
class Section;
class VerticalAxisRangeControls;

namespace Ui {
class OnionSkinViewPropertiesWidget;
}

/**
 * @brief Properties panel for Onion Skin View Widget
 */
class OnionSkinViewPropertiesWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct an OnionSkinViewPropertiesWidget
     * @param state Shared state with the view widget
     * @param data_manager DataManager for data queries
     * @param parent Parent widget
     */
    explicit OnionSkinViewPropertiesWidget(
        std::shared_ptr<OnionSkinViewState> state,
        std::shared_ptr<DataManager> data_manager,
        QWidget * parent = nullptr);

    ~OnionSkinViewPropertiesWidget() override;

    [[nodiscard]] std::shared_ptr<OnionSkinViewState> state() const { return _state; }
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    /**
     * @brief Set the OnionSkinViewWidget to connect axis range controls
     * @param plot_widget The OnionSkinViewWidget instance
     */
    void setPlotWidget(OnionSkinViewWidget * plot_widget);

private slots:
    // Point data key management
    void _onAddPointClicked();
    void _onRemovePointClicked();
    void _onPointTableSelectionChanged();

    // Line data key management
    void _onAddLineClicked();
    void _onRemoveLineClicked();
    void _onLineTableSelectionChanged();

    // Mask data key management
    void _onAddMaskClicked();
    void _onRemoveMaskClicked();
    void _onMaskTableSelectionChanged();

    // Temporal window controls
    void _onWindowBehindChanged(int value);
    void _onWindowAheadChanged(int value);

    // Alpha curve controls
    void _onAlphaCurveChanged(int index);
    void _onMinAlphaChanged(double value);
    void _onMaxAlphaChanged(double value);

    // Rendering controls
    void _onPointSizeChanged(double value);
    void _onLineWidthChanged(double value);
    void _onHighlightCurrentChanged(bool checked);

    // State change handlers
    void _onStatePointKeyAdded(QString const & key);
    void _onStatePointKeyRemoved(QString const & key);
    void _onStateLineKeyAdded(QString const & key);
    void _onStateLineKeyRemoved(QString const & key);
    void _onStateMaskKeyAdded(QString const & key);
    void _onStateMaskKeyRemoved(QString const & key);

private:
    void _populatePointComboBox();
    void _populateLineComboBox();
    void _populateMaskComboBox();
    void _updatePointDataTable();
    void _updateLineDataTable();
    void _updateMaskDataTable();
    void _updateUIFromState();

    Ui::OnionSkinViewPropertiesWidget * ui;
    std::shared_ptr<OnionSkinViewState> _state;
    std::shared_ptr<DataManager> _data_manager;
    OnionSkinViewWidget * _plot_widget;
    HorizontalAxisRangeControls * _horizontal_range_controls;
    Section * _horizontal_range_controls_section;
    VerticalAxisRangeControls * _vertical_range_controls;
    Section * _vertical_range_controls_section;

    /// DataManager observer callback ID (stored for cleanup)
    int _dm_observer_id = -1;
};

#endif  // ONION_SKIN_VIEW_PROPERTIES_WIDGET_HPP
