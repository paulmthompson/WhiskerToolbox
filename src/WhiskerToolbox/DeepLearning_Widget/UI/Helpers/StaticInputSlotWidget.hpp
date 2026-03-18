/// @file StaticInputSlotWidget.hpp
/// @brief Self-contained widget for configuring one non-sequence static input slot.
///
/// Replaces the non-sequence branch of `_buildStaticInputGroup()` with a
/// schema-driven form powered by AutoParamWidget. The widget owns a
/// `StaticInputSlotParams` struct, manages the capture button lifecycle,
/// and exposes its state via signals.

#ifndef STATIC_INPUT_SLOT_WIDGET_HPP
#define STATIC_INPUT_SLOT_WIDGET_HPP

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <utility>

class AutoParamWidget;
class DataManager;
class QLabel;
class QPushButton;
struct StaticInputData;

namespace dl {
struct TensorSlotDescriptor;
}

namespace dl::widget {

/**
 * @brief Widget for configuring a single non-sequence static (memory) input slot.
 *
 * Wraps an AutoParamWidget driven by the `StaticInputSlotParams` schema.
 * The data-source combo is populated dynamically from DataManager.
 * A capture button appears only when Absolute capture mode is active.
 *
 * @note Not thread-safe — must be used from the GUI thread only.
 */
class StaticInputSlotWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a widget for one non-sequence static input slot.
     * @param slot    Descriptor for the slot (name, shape, recommended encoder).
     * @param dm      Shared DataManager for populating the data-source combo.
     * @param parent  Optional parent widget.
     *
     * @pre dm must not be null.
     */
    explicit StaticInputSlotWidget(
            dl::TensorSlotDescriptor const & slot,
            std::shared_ptr<DataManager> dm,
            QWidget * parent = nullptr);

    ~StaticInputSlotWidget() override;

    // Non-copyable, non-movable (QWidget)
    StaticInputSlotWidget(StaticInputSlotWidget const &) = delete;
    StaticInputSlotWidget & operator=(StaticInputSlotWidget const &) = delete;
    StaticInputSlotWidget(StaticInputSlotWidget &&) = delete;
    StaticInputSlotWidget & operator=(StaticInputSlotWidget &&) = delete;

    /// @brief Return the current parameter values.
    [[nodiscard]] StaticInputSlotParams params() const;

    /// @brief Set the parameter values and update the UI.
    void setParams(StaticInputSlotParams const & params);

    /// @brief Refresh the data-source combo with current DataManager keys.
    void refreshDataSources();

    /// @brief Return the slot name this widget is bound to.
    [[nodiscard]] std::string const & slotName() const;

    /// @brief Convert current parameters to a StaticInputData for state sync.
    ///
    /// The returned `captured_frame` equals the value set by the last
    /// `setCapturedStatus()` call, or -1 if capture was never performed.
    /// Callers should preserve the existing `captured_frame` from state when
    /// the widget value is -1 (panel was freshly rebuilt without a recapture).
    ///
    /// @post result.memory_index == 0
    [[nodiscard]] StaticInputData toStaticInputData() const;

    /// @brief Update the capture status display after a successful capture.
    ///
    /// Called by the parent widget after `SlotAssembler::captureStaticInput()` succeeds.
    ///
    /// @param captured_frame  Frame index that was encoded and cached.
    /// @param value_range     {min, max} value range of the cached tensor.
    void setCapturedStatus(int captured_frame,
                           std::pair<float, float> value_range);

    /// @brief Reset the capture status display to "Not captured".
    void clearCapturedStatus();

    /// @brief Enable or disable the capture button based on model readiness.
    ///
    /// Called by the parent when the model loads or unloads.
    void setModelReady(bool ready);

signals:
    /// Emitted whenever any parameter in the slot changes.
    void bindingChanged();

    /// Emitted when the user clicks the capture button.
    ///
    /// The parent should call `SlotAssembler::captureStaticInput()` and then
    /// call `setCapturedStatus()` with the result or `clearCapturedStatus()`
    /// on failure.
    void captureRequested(std::string slot_name);

    /// Emitted when the data source changes, requiring cache invalidation.
    ///
    /// The parent should call `SlotAssembler::clearStaticCacheEntry()` and
    /// emit `staticCacheChanged()`.
    void captureInvalidated(std::string slot_name);

private:
    /// Populate the source combo with DM keys matching the slot's encoder type.
    void _refreshSourceCombo();

    /// Update the capture button's enabled state based on mode + model readiness.
    void _updateCaptureButtonEnabled();

    /// Return true if the current capture_mode variant is AbsoluteCaptureParams.
    [[nodiscard]] bool _isAbsoluteMode() const;

    std::string _slot_name;
    std::string _recommended_encoder;
    std::shared_ptr<DataManager> _dm;
    AutoParamWidget * _auto_param = nullptr;
    QPushButton * _capture_btn = nullptr;
    QLabel * _capture_status = nullptr;
    QWidget * _capture_row_widget = nullptr;///< Container for capture button + status label
    int _captured_frame = -1;
    bool _model_ready = false;
    std::string _last_source;///< Tracks the previous source to detect cache-invalidating changes
};

}// namespace dl::widget

#endif// STATIC_INPUT_SLOT_WIDGET_HPP
