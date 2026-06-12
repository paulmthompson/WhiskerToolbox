/// @file SequenceSlotWidget.hpp
/// @brief Self-contained widget for configuring one sequence input slot.
///
/// Replaces the sequence branch of `_buildStaticInputGroup()` with a
/// schema-driven form. Each sequence entry is an AutoParamWidget driven by
/// `SequenceEntryVariant`.

#ifndef SEQUENCE_SLOT_WIDGET_HPP
#define SEQUENCE_SLOT_WIDGET_HPP

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include <QWidget>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class AutoParamWidget;
class DataManager;
class QGroupBox;
class QPushButton;
struct RecurrentBindingData;
struct StaticInputData;

namespace dl {
struct TensorSlotDescriptor;
}

namespace dl::widget {

/**
 * @brief Widget for configuring a single sequence (memory) input slot.
 *
 * Manages a list of sequence entries, each driven by SequenceEntryVariant.
 * Each entry can be Static (DataManager or DataBank source) or Recurrent
 * (feedback from model output). Add/Remove buttons control the entry count.
 *
 * @note Not thread-safe — must be used from the GUI thread only.
 */
class SequenceSlotWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a widget for one sequence static input slot.
     * @param slot             Descriptor for the slot (name, shape, sequence_dim).
     * @param dm               Shared DataManager for populating data_key combos.
     * @param output_slot_names Model output slot names for recurrent combo.
     * @param parent            Optional parent widget.
     *
     * @pre dm must not be null.
     */
    explicit SequenceSlotWidget(
            dl::TensorSlotDescriptor const & slot,
            std::shared_ptr<DataManager> dm,
            std::vector<std::string> output_slot_names,
            QWidget * parent = nullptr);

    ~SequenceSlotWidget() override;

    // Non-copyable, non-movable (QWidget)
    SequenceSlotWidget(SequenceSlotWidget const &) = delete;
    SequenceSlotWidget & operator=(SequenceSlotWidget const &) = delete;
    SequenceSlotWidget(SequenceSlotWidget &&) = delete;
    SequenceSlotWidget & operator=(SequenceSlotWidget &&) = delete;

    /// @brief Return the slot name this widget is bound to.
    [[nodiscard]] std::string const & slotName() const;

    /// @brief Refresh data_key combos from DataManager.
    void refreshDataSources();

    /// @brief Refresh bank_entry_id combos with current DataBank entry IDs.
    void refreshBankEntries(std::vector<std::string> const & bank_entry_ids);

    /// @brief Refresh output_slot_name combos with current model output names.
    void refreshOutputSlots(std::vector<std::string> const & output_slot_names);

    /// @brief Collect static inputs from Static-type entries.
    [[nodiscard]] std::vector<StaticInputData> getStaticInputs() const;

    /// @brief Collect recurrent bindings from Recurrent-type entries.
    [[nodiscard]] std::vector<RecurrentBindingData> getRecurrentBindings() const;

    /// @brief Set initial entries from existing state (static inputs + recurrent).
    void setEntriesFromState(
            std::vector<StaticInputData> const & static_inputs,
            std::vector<RecurrentBindingData> const & recurrent_bindings);

signals:
    /// Emitted whenever any parameter in any entry changes.
    void bindingChanged();

private:
    struct EntryRow {
        QGroupBox * group = nullptr;
        AutoParamWidget * auto_param = nullptr;
        int memory_index = 0;
    };

    void _addEntryRow(int memory_index);
    void _removeLastEntry();
    void _refreshDataKeyCombos();
    void _refreshOutputSlotCombos();

    std::string _slot_name;
    std::string _recommended_encoder;
    int64_t _seq_len;
    std::shared_ptr<DataManager> _dm;
    std::vector<std::string> _output_slot_names;

    QWidget * _entries_container = nullptr;
    std::vector<EntryRow> _entry_rows;
    QPushButton * _add_btn = nullptr;
    QPushButton * _remove_btn = nullptr;
};

}// namespace dl::widget

#endif// SEQUENCE_SLOT_WIDGET_HPP
