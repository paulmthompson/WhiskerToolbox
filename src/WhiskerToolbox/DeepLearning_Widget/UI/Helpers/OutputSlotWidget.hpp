/// @file OutputSlotWidget.hpp
/// @brief Self-contained widget for configuring one output slot.
///
/// Replaces the hand-built `_buildOutputGroup()` panel with a
/// schema-driven form powered by AutoParamWidget. The widget owns an
/// `OutputSlotParams` struct and exposes it via toOutputBindingData().

#ifndef OUTPUT_SLOT_WIDGET_HPP
#define OUTPUT_SLOT_WIDGET_HPP

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class AutoParamWidget;
class DataManager;
class QGroupBox;
class QLabel;
struct OutputBindingData;

namespace dl {
struct TensorSlotDescriptor;
}

namespace dl::widget {

/**
 * @brief Widget for configuring a single model output slot.
 *
 * Wraps an AutoParamWidget driven by the `OutputSlotParams` schema.
 * The target (data_key) combo is populated dynamically from DataManager
 * based on the decoder's expected output type.
 *
 * @note Not thread-safe — must be used from the GUI thread only.
 */
class OutputSlotWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a widget for one output slot.
     * @param slot   Descriptor for the slot (name, shape, recommended decoder).
     * @param dm     Shared DataManager for populating the target combo.
     * @param parent Optional parent widget.
     *
     * @pre dm must not be null.
     */
    explicit OutputSlotWidget(
            dl::TensorSlotDescriptor const & slot,
            std::shared_ptr<DataManager> dm,
            QWidget * parent = nullptr);

    ~OutputSlotWidget() override;

    // Non-copyable, non-movable (QWidget)
    OutputSlotWidget(OutputSlotWidget const &) = delete;
    OutputSlotWidget & operator=(OutputSlotWidget const &) = delete;
    OutputSlotWidget(OutputSlotWidget &&) = delete;
    OutputSlotWidget & operator=(OutputSlotWidget &&) = delete;

    /// @brief Return the current parameter values.
    [[nodiscard]] OutputSlotParams params() const;

    /// @brief Set the parameter values and update the UI.
    void setParams(OutputSlotParams const & params);

    /// @brief Refresh the target combo with current DataManager keys.
    void refreshDataSources();

    /// @brief Restrict visible decoder alternatives (e.g. when post-encoder changes).
    void updateDecoderAlternatives(std::vector<std::string> const & allowed_tags);

    /// @brief Return the slot name this widget is bound to.
    [[nodiscard]] std::string const & slotName() const;

    /// @brief Convert current parameters to OutputBindingData for SlotAssembler.
    [[nodiscard]] OutputBindingData toOutputBindingData() const;

    /// @brief Build OutputSlotParams from saved OutputBindingData (for state restore).
    [[nodiscard]] static OutputSlotParams paramsFromBinding(
            OutputBindingData const & binding);

signals:
    /// Emitted whenever any parameter in the slot changes.
    void bindingChanged();

private:
    /// Populate the target combo with DM keys matching the decoder's output type.
    void _refreshTargetCombo();

    /// Map decoder variant tag to SlotAssembler decoder ID string.
    [[nodiscard]] static std::string _decoderIdFromTag(std::string const & json);

    std::string _slot_name;
    std::shared_ptr<DataManager> _dm;

    AutoParamWidget * _auto_param = nullptr;

    /// Recommended decoder from slot descriptor, used for initial selection.
    std::string _recommended_decoder;
};

}// namespace dl::widget

#endif// OUTPUT_SLOT_WIDGET_HPP
