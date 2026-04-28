/**
 * @file DynamicInputSlotWidget.hpp
 * @brief Self-contained widget for configuring one dynamic input slot.
 * 
 * Replaces the hand-built `_buildDynamicInputGroup()` panel with a
 * schema-driven form powered by AutoParamWidget. The widget owns a
 * `DynamicInputSlotParams` struct and exposes it via signals.
 */

#ifndef DYNAMIC_INPUT_SLOT_WIDGET_HPP
#define DYNAMIC_INPUT_SLOT_WIDGET_HPP

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class AutoParamWidget;
class DataManager;
class QGroupBox;
class QLabel;
struct SlotBindingData;

namespace dl {
struct TensorSlotDescriptor;
}

namespace dl::widget {

/**
 * @brief Widget for configuring a single dynamic (per-frame) input slot.
 *
 * Wraps an AutoParamWidget driven by the `DynamicInputSlotParams` schema.
 * The data-source combo is populated dynamically from DataManager, and
 * encoder alternatives are constrained to types compatible with the
 * selected data source.
 *
 * @note Not thread-safe — must be used from the GUI thread only.
 */
class DynamicInputSlotWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a widget for one dynamic input slot.
     * @param slot  Descriptor for the slot (name, shape, recommended encoder).
     * @param dm    Shared DataManager for populating the data-source combo.
     * @param parent  Optional parent widget.
     *
     * @pre dm must not be null.
     */
    explicit DynamicInputSlotWidget(
            dl::TensorSlotDescriptor const & slot,
            std::shared_ptr<DataManager> dm,
            QWidget * parent = nullptr);

    ~DynamicInputSlotWidget() override;

    // Non-copyable, non-movable (QWidget)
    DynamicInputSlotWidget(DynamicInputSlotWidget const &) = delete;
    DynamicInputSlotWidget & operator=(DynamicInputSlotWidget const &) = delete;
    DynamicInputSlotWidget(DynamicInputSlotWidget &&) = delete;
    DynamicInputSlotWidget & operator=(DynamicInputSlotWidget &&) = delete;

    /**
     * @brief Return the current parameter values.
     */
    [[nodiscard]] DynamicInputSlotParams params() const;

    /**
     * @brief Set the parameter values and update the UI.
     */
    void setParams(DynamicInputSlotParams const & params);

    /**
     * @brief Refresh the data-source combo with current DataManager keys.
     */
    void refreshDataSources();

    /**
     * @brief Return the slot name this widget is bound to.
     */
    [[nodiscard]] std::string const & slotName() const;

    /**
     * @brief Convert current parameters to a SlotBindingData for SlotAssembler.
     * 
     * Extracts encoder_id, mode, gaussian_sigma from the EncoderVariant.
     */
    [[nodiscard]] SlotBindingData toSlotBindingData() const;

signals:
    /**
     * @brief Emitted whenever any parameter in the slot changes.
     */
    void bindingChanged();

private:
    /**
     * @brief Populate the source combo with DM keys matching all dynamic input types.
     */
    void _refreshSourceCombo();

    /**
     * @brief Map an encoder tag to the DM data types it can consume.
     */
    [[nodiscard]] static std::vector<std::string> _encoderTagsForDataType(
            std::string const & data_type_hint);

    /**
     * @brief Map an encoder variant tag to its SlotAssembler encoder ID string.
     */
    [[nodiscard]] static std::string _encoderIdFromTag(
            std::string const & json);

    std::string _slot_name;
    std::shared_ptr<DataManager> _dm;

    AutoParamWidget * _auto_param = nullptr;

    /// Recommended encoder from slot descriptor, used for initial selection.
    std::string _recommended_encoder;
};

}// namespace dl::widget

#endif// DYNAMIC_INPUT_SLOT_WIDGET_HPP
