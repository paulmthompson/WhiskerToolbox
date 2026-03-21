/// @file SequenceSlotWidget.hpp
/// @brief Self-contained widget for configuring one sequence input slot.
///
/// Replaces the sequence branch of `_buildStaticInputGroup()` with a
/// schema-driven form. Each sequence entry is an AutoParamWidget driven by
/// `SequenceEntryVariant`. Manages add/remove entry rows and capture for
/// Static+Absolute entries.

#ifndef SEQUENCE_SLOT_WIDGET_HPP
#define SEQUENCE_SLOT_WIDGET_HPP

#include "DeepLearning_Widget/Core/DeepLearningParamSchemas.hpp"

#include <QWidget>

#include <memory>
#include <string>
#include <utility>
#include <vector>

class AutoParamWidget;
class DataManager;
class QGroupBox;
class QLabel;
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
 * Each entry can be Static (data from DataManager) or Recurrent (feedback
 * from model output). Add/Remove buttons control the entry count. Capture
 * button appears for Static+Absolute entries.
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

    /// @brief Refresh output_slot_name combos with current model output names.
    void refreshOutputSlots(std::vector<std::string> const & output_slot_names);

    /// @brief Collect static inputs from Static-type entries.
    ///
    /// Returns StaticInputData for each entry where source_type is Static.
    /// captured_frame is taken from the last setCapturedStatus() for that index.
    [[nodiscard]] std::vector<StaticInputData> getStaticInputs() const;

    /// @brief Collect recurrent bindings from Recurrent-type entries.
    ///
    /// Returns RecurrentBindingData for each entry where source_type is
    /// Recurrent and output_slot_name is non-empty.
    [[nodiscard]] std::vector<RecurrentBindingData> getRecurrentBindings() const;

    /// @brief Update the capture status display for an entry after capture.
    void setCapturedStatus(int memory_index,
                           int captured_frame,
                           std::pair<float, float> value_range);

    /// @brief Reset the capture status for an entry.
    void clearCapturedStatus(int memory_index);

    /// @brief Enable or disable capture buttons based on model readiness.
    void setModelReady(bool ready);

    /// @brief Set initial entries from existing state (static inputs + recurrent).
    ///
    /// Called by parent after construction to restore saved bindings.
    void setEntriesFromState(
            std::vector<StaticInputData> const & static_inputs,
            std::vector<RecurrentBindingData> const & recurrent_bindings);

signals:
    /// Emitted whenever any parameter in any entry changes.
    void bindingChanged();

    /// Emitted when the user clicks a capture button.
    void captureRequested(std::string slot_name, int memory_index);

    /// Emitted when data_key changes for an entry (parent should clear cache).
    void captureInvalidated(std::string slot_name, int memory_index);

private:
    struct EntryRow {
        QGroupBox * group = nullptr;
        AutoParamWidget * auto_param = nullptr;
        QWidget * capture_row = nullptr;
        QPushButton * capture_btn = nullptr;
        QLabel * capture_status = nullptr;
        int memory_index = 0;
        int captured_frame = -1;
        std::string last_data_key;
    };

    /// Add one entry row at the given memory index.
    void _addEntryRow(int memory_index);

    /// Remove the last entry row.
    void _removeLastEntry();

    /// Refresh data_key for all entry AutoParamWidgets.
    void _refreshDataKeyCombos();

    /// Refresh output_slot_name for all entry AutoParamWidgets.
    void _refreshOutputSlotCombos();

    /// Update capture row visibility and enabled state for an entry.
    void _updateEntryCaptureRow(EntryRow & row);

    /// Return true if the given params represent Static+Absolute mode.
    [[nodiscard]] static bool _isStaticAbsolute(
            SequenceEntryVariant const & params);

    std::string _slot_name;
    std::string _recommended_encoder;
    int64_t _seq_len;
    std::shared_ptr<DataManager> _dm;
    std::vector<std::string> _output_slot_names;
    bool _model_ready = false;

    QWidget * _entries_container = nullptr;
    std::vector<EntryRow> _entry_rows;
    QPushButton * _add_btn = nullptr;
    QPushButton * _remove_btn = nullptr;
};

}// namespace dl::widget

#endif// SEQUENCE_SLOT_WIDGET_HPP
