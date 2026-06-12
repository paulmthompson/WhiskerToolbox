/// @file StaticInputSlotWidget.hpp
/// @brief Self-contained widget for configuring one non-sequence static input slot.
///
/// Replaces the non-sequence branch of `_buildStaticInputGroup()` with a
/// schema-driven form powered by AutoParamWidget.

#ifndef STATIC_INPUT_SLOT_WIDGET_HPP
#define STATIC_INPUT_SLOT_WIDGET_HPP

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <vector>

class AutoParamWidget;
class DataManager;
class QLabel;
struct StaticInputData;

namespace dl {
struct TensorSlotDescriptor;
}

namespace dl::widget {

/**
 * @brief Widget for configuring a single non-sequence static (memory) input slot.
 *
 * Wraps an AutoParamWidget driven by the `StaticInputSlotParams` schema.
 * The data-source and bank-entry combos are populated dynamically.
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

    /// @brief Refresh the bank-entry combo with current DataBank entry IDs.
    void refreshBankEntries(std::vector<std::string> const & bank_entry_ids);

    /// @brief Return the slot name this widget is bound to.
    [[nodiscard]] std::string const & slotName() const;

    /// @brief Convert current parameters to a StaticInputData for state sync.
    ///
    /// @post result.memory_index == 0
    [[nodiscard]] StaticInputData toStaticInputData() const;

signals:
    /// Emitted whenever any parameter in the slot changes.
    void bindingChanged();

private:
    /// Populate the source combo with DM keys matching the slot's encoder type.
    void _refreshSourceCombo();

    std::string _slot_name;
    std::string _recommended_encoder;
    std::shared_ptr<DataManager> _dm;
    AutoParamWidget * _auto_param = nullptr;
};

}// namespace dl::widget

#endif// STATIC_INPUT_SLOT_WIDGET_HPP
