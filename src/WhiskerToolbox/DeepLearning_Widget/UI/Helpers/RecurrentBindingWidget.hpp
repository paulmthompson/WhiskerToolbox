/// @file RecurrentBindingWidget.hpp
/// @brief Self-contained widget for configuring one non-sequence recurrent
///        (feedback) binding.
///
/// Replaces the hand-built `_buildRecurrentInputGroup()` panel with a
/// schema-driven form powered by AutoParamWidget. The widget owns a
/// `RecurrentBindingSlotParams` struct and exposes it via
/// toRecurrentBindingData().

#ifndef RECURRENT_BINDING_WIDGET_HPP
#define RECURRENT_BINDING_WIDGET_HPP

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class AutoParamWidget;
class DataManager;
class QGroupBox;
class QLabel;
struct RecurrentBindingData;

namespace dl {
struct TensorSlotDescriptor;
}

namespace dl::widget {

/**
 * @brief Widget for configuring a single non-sequence recurrent binding.
 *
 * Maps a model output slot to an input slot for sequential frame-by-frame
 * processing. The output at frame t becomes the input at frame t+1.
 *
 * Wraps an AutoParamWidget driven by the `RecurrentBindingSlotParams` schema.
 * The output_slot_name combo is populated from model output slot names.
 * The data_key combo (inside StaticCapture init) is populated from DataManager.
 *
 * @note Not thread-safe — must be used from the GUI thread only.
 */
class RecurrentBindingWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a widget for one recurrent binding.
     * @param input_slot  Descriptor for the input slot being fed (recurrent target).
     * @param output_slots  Model output slot descriptors for the output combo.
     * @param dm  Shared DataManager for populating the data_key combo (StaticCapture).
     * @param parent  Optional parent widget.
     *
     * @pre dm must not be null.
     */
    explicit RecurrentBindingWidget(
            dl::TensorSlotDescriptor const & input_slot,
            std::vector<dl::TensorSlotDescriptor> const & output_slots,
            std::shared_ptr<DataManager> dm,
            QWidget * parent = nullptr);

    ~RecurrentBindingWidget() override;

    // Non-copyable, non-movable (QWidget)
    RecurrentBindingWidget(RecurrentBindingWidget const &) = delete;
    RecurrentBindingWidget & operator=(RecurrentBindingWidget const &) = delete;
    RecurrentBindingWidget(RecurrentBindingWidget &&) = delete;
    RecurrentBindingWidget & operator=(RecurrentBindingWidget &&) = delete;

    /// @brief Return the current parameter values.
    [[nodiscard]] RecurrentBindingSlotParams params() const;

    /// @brief Set the parameter values and update the UI.
    void setParams(RecurrentBindingSlotParams const & params);

    /// @brief Refresh the output_slot_name combo with current output slot names.
    void refreshOutputSlots(std::vector<std::string> const & output_names);

    /// @brief Refresh the data_key combo (StaticCapture init) from DataManager.
    void refreshDataSources();

    /// @brief Return the input slot name this widget is bound to.
    [[nodiscard]] std::string const & slotName() const;

    /// @brief Convert current parameters to RecurrentBindingData for SlotAssembler.
    [[nodiscard]] RecurrentBindingData toRecurrentBindingData() const;

    /// @brief Build RecurrentBindingSlotParams from saved RecurrentBindingData.
    [[nodiscard]] static RecurrentBindingSlotParams paramsFromBinding(
            RecurrentBindingData const & binding);

signals:
    /// Emitted whenever any parameter in the binding changes.
    void bindingChanged();

private:
    /// Populate the output_slot_name combo.
    void _refreshOutputSlotCombo();

    /// Populate the data_key combo (StaticCapture init variant).
    void _refreshDataKeyCombo();

    std::string _input_slot_name;
    std::shared_ptr<DataManager> _dm;
    std::vector<std::string> _output_slot_names;
    std::string _recommended_encoder;

    AutoParamWidget * _auto_param = nullptr;
};

}// namespace dl::widget

#endif// RECURRENT_BINDING_WIDGET_HPP
