/// @file OutputSlotWidget.hpp
/// @brief Self-contained widget for configuring one output slot.
///
/// Schema-driven form powered by AutoParamWidget over `dl::OutputBindingForm`,
/// exposed as `OutputBindingData`.

#ifndef OUTPUT_SLOT_WIDGET_HPP
#define OUTPUT_SLOT_WIDGET_HPP

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
 * @brief Widget for configuring a single model output slot.
 *
 * Wraps an AutoParamWidget driven by `dl::OutputBindingForm`.
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

    OutputSlotWidget(OutputSlotWidget const &) = delete;
    OutputSlotWidget & operator=(OutputSlotWidget const &) = delete;
    OutputSlotWidget(OutputSlotWidget &&) = delete;
    OutputSlotWidget & operator=(OutputSlotWidget &&) = delete;

    /**
     * @brief Return the current slot binding (includes @c slot_name).
     */
    [[nodiscard]] OutputBindingData binding() const;

    /**
     * @brief Set binding values and update the UI.
     */
    void setBinding(OutputBindingData const & binding);

    /// @brief Refresh the target combo with current DataManager keys.
    void refreshDataSources();

    /// @brief Restrict visible decoder alternatives (e.g. when post-encoder changes).
    void updateDecoderAlternatives(std::vector<std::string> const & allowed_tags);

    /// @brief Return the slot name this widget is bound to.
    [[nodiscard]] std::string const & slotName() const;

signals:
    /// Emitted whenever any parameter in the slot changes.
    void bindingChanged();

private:
    [[nodiscard]] OutputBindingForm _form() const;
    void _setForm(OutputBindingForm const & form);
    void _refreshTargetCombo();

    std::string _slot_name;
    std::shared_ptr<DataManager> _dm;

    AutoParamWidget * _auto_param = nullptr;

    std::string _recommended_decoder;
};

}// namespace dl::widget

#endif// OUTPUT_SLOT_WIDGET_HPP
