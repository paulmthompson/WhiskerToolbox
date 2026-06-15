/**
 * @file DynamicInputSlotWidget.hpp
 * @brief Self-contained widget for configuring one dynamic input slot.
 *
 * Schema-driven form powered by AutoParamWidget over
 * `dl::DynamicInputBindingForm`, exposed as `SlotBindingData`.
 */

#ifndef DYNAMIC_INPUT_SLOT_WIDGET_HPP
#define DYNAMIC_INPUT_SLOT_WIDGET_HPP

#include "DeepLearning/bindings/BindingParamSchemas.hpp"
#include "DeepLearning/bindings/SlotBindingTypes.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class AutoParamWidget;
class DataManager;
class QGroupBox;
class QLabel;

namespace dl {
struct TensorSlotDescriptor;
}

namespace dl::widget {

/**
 * @brief Widget for configuring a single dynamic (per-frame) input slot.
 *
 * Wraps an AutoParamWidget driven by `dl::DynamicInputBindingForm`.
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

    DynamicInputSlotWidget(DynamicInputSlotWidget const &) = delete;
    DynamicInputSlotWidget & operator=(DynamicInputSlotWidget const &) = delete;
    DynamicInputSlotWidget(DynamicInputSlotWidget &&) = delete;
    DynamicInputSlotWidget & operator=(DynamicInputSlotWidget &&) = delete;

    /**
     * @brief Return the current slot binding (includes @c slot_name).
     */
    [[nodiscard]] SlotBindingData binding() const;

    /**
     * @brief Set binding values and update the UI.
     * @param binding Saved binding; @c slot_name may differ from this widget's slot.
     */
    void setBinding(SlotBindingData const & binding);

    /**
     * @brief Refresh the data-source combo with current DataManager keys.
     */
    void refreshDataSources();

    /**
     * @brief Return the slot name this widget is bound to.
     */
    [[nodiscard]] std::string const & slotName() const;

signals:
    /**
     * @brief Emitted whenever any parameter in the slot changes.
     */
    void bindingChanged();

private:
    /**
     * @brief Read the current form values from AutoParamWidget.
     */
    [[nodiscard]] DynamicInputBindingForm _form() const;

    /**
     * @brief Write form values to AutoParamWidget.
     */
    void _setForm(DynamicInputBindingForm const & form);

    /**
     * @brief Populate the data_key combo with DM keys for dynamic input types.
     */
    void _refreshSourceCombo();

    std::string _slot_name;
    std::shared_ptr<DataManager> _dm;

    AutoParamWidget * _auto_param = nullptr;

    std::string _recommended_encoder;
};

}// namespace dl::widget

#endif// DYNAMIC_INPUT_SLOT_WIDGET_HPP
