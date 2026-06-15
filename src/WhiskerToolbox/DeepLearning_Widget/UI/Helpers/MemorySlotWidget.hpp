/// @file MemorySlotWidget.hpp
/// @brief Self-contained widget for configuring one memory (static) input slot.

#ifndef MEMORY_SLOT_WIDGET_HPP
#define MEMORY_SLOT_WIDGET_HPP

#include "DeepLearning/bindings/BindingParamSchemas.hpp"
#include "DeepLearning/bindings/DeepLearningBindingData.hpp"

#include <QWidget>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class AutoParamWidget;
class DataManager;
class QComboBox;
class QGroupBox;
class QPushButton;
class QStackedWidget;

namespace dl {
struct TensorSlotDescriptor;
}

namespace dl::widget {

/**
 * @brief Widget for configuring memory frame bindings for one static input slot.
 *
 * Supports both single-frame (n=1) and sequence (n>1) memory slots. Each
 * position is either static (DataManager/DataBank) or recurrent feedback.
 *
 * @note Not thread-safe — must be used from the GUI thread only.
 */
class MemorySlotWidget : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Construct a widget for one memory input slot.
     * @param slot              Descriptor for the slot (name, shape, sequence_dim).
     * @param dm                Shared DataManager for populating data_key combos.
     * @param output_slot_names Model output slot names for recurrent combo.
     * @param parent            Optional parent widget.
     *
     * @pre dm must not be null.
     */
    explicit MemorySlotWidget(
            dl::TensorSlotDescriptor const & slot,
            std::shared_ptr<DataManager> dm,
            std::vector<std::string> output_slot_names,
            QWidget * parent = nullptr);

    ~MemorySlotWidget() override;

    MemorySlotWidget(MemorySlotWidget const &) = delete;
    MemorySlotWidget & operator=(MemorySlotWidget const &) = delete;
    MemorySlotWidget(MemorySlotWidget &&) = delete;
    MemorySlotWidget & operator=(MemorySlotWidget &&) = delete;

    /// @brief Return the slot name this widget is bound to.
    [[nodiscard]] std::string const & slotName() const;

    /// @brief Memory frame bindings for this slot only.
    [[nodiscard]] std::vector<MemoryFrameBinding> bindings() const;

    /**
     * @brief Restore bindings for this slot from session state.
     * @param all_frames Full session vector; entries for other slots are ignored.
     */
    void setBindings(std::vector<MemoryFrameBinding> const & all_frames);

    /// @brief Refresh data_key combos from DataManager.
    void refreshDataSources();

    /// @brief Refresh bank_entry_id combos with current DataBank entry IDs.
    void refreshBankEntries(std::vector<std::string> const & bank_entry_ids);

    /// @brief Refresh output_slot_name combos with current model output names.
    void refreshOutputSlots(std::vector<std::string> const & output_slot_names);

signals:
    /// Emitted whenever any parameter in any entry changes.
    void bindingChanged();

private:
    struct EntryRow {
        QGroupBox * group = nullptr;
        QComboBox * kind_combo = nullptr;
        QStackedWidget * stack = nullptr;
        AutoParamWidget * static_param = nullptr;
        AutoParamWidget * recurrent_param = nullptr;
        int memory_index = 0;
    };

    void _addEntryRow(int memory_index);
    void _removeLastEntry();
    void _refreshDataKeyCombos();
    void _refreshOutputSlotCombos();
    void _onEntryKindChanged(EntryRow & row);

    [[nodiscard]] MemoryFrameBinding _bindingFromRow(EntryRow const & row) const;
    void _setRowFromBinding(EntryRow & row, MemoryFrameBinding const & binding);

    std::string _slot_name;
    std::string _recommended_encoder;
    int64_t _seq_len = 1;
    std::shared_ptr<DataManager> _dm;
    std::vector<std::string> _output_slot_names;

    QWidget * _entries_container = nullptr;
    std::vector<EntryRow> _entry_rows;
    QPushButton * _add_btn = nullptr;
    QPushButton * _remove_btn = nullptr;
};

} // namespace dl::widget

#endif // MEMORY_SLOT_WIDGET_HPP
