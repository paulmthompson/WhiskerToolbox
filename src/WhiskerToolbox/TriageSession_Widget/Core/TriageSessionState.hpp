/**
 * @file TriageSessionState.hpp
 * @brief EditorState subclass for the TriageSession widget
 *
 * Serializes the triage pipeline JSON and tracked-regions key.
 * The TriageSession state machine always resets to Idle on workspace load.
 *
 * @see EditorState for base class documentation
 * @see TriageSession for the non-Qt state machine
 */

#ifndef TRIAGE_SESSION_STATE_HPP
#define TRIAGE_SESSION_STATE_HPP

#include "EditorState/EditorState.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class DataManager;

struct TriageHotkeySlotData {
    int slot_index = 1;                      ///< 1-based slot index; identity is stable
    std::string display_name = "";           ///< User-visible name for this slot
    bool enabled = true;                     ///< Whether this slot can be triggered
    std::optional<std::string> sequence_json;///< Serialized CommandSequenceDescriptor JSON
};

/**
 * @brief Serializable data structure for TriageSessionState
 *
 * The TriageSession state machine (Idle/Marking) is intentionally NOT
 * serialized — it always starts Idle on workspace load.
 */
struct TriageSessionStateData {
    std::string instance_id;                    ///< Unique instance ID (preserved across serialization)
    std::string display_name = "Triage Session";///< User-visible name

    std::optional<std::string> pipeline_json;           ///< Legacy single-slot JSON retained for migration compatibility
    std::vector<TriageHotkeySlotData> sequence_slots;   ///< Runtime-editable sequence slots (expected size: 9)
    std::string tracked_regions_key = "tracked_regions";///< Key to the tracked-regions DigitalIntervalSeries
};

/**
 * @brief EditorState subclass for the TriageSession widget
 *
 * Manages serializable state for the triage session properties panel.
 * The underlying TriageSession state machine is NOT part of this state —
 * it always resets to Idle on workspace load/restore.
 */
class TriageSessionState : public EditorState {
    Q_OBJECT

public:
    explicit TriageSessionState(std::shared_ptr<DataManager> data_manager = nullptr,
                                QObject * parent = nullptr);
    ~TriageSessionState() override = default;

    // === Type Identification ===
    [[nodiscard]] QString getTypeName() const override { return QStringLiteral("TriageSessionWidget"); }
    [[nodiscard]] QString getDisplayName() const override;
    void setDisplayName(QString const & name) override;

    // === Serialization ===
    [[nodiscard]] std::string toJson() const override;
    bool fromJson(std::string const & json) override;

    // === Accessors ===
    [[nodiscard]] std::shared_ptr<DataManager> dataManager() const { return _data_manager; }

    // Pipeline JSON
    void setPipelineJson(std::string_view json);
    [[nodiscard]] std::optional<std::string> pipelineJson() const { return _data.pipeline_json; }

    // Tracked regions key
    void setTrackedRegionsKey(std::string_view key);
    [[nodiscard]] std::string trackedRegionsKey() const { return _data.tracked_regions_key; }

    // Sequence slots
    [[nodiscard]] static constexpr int sequenceSlotCount() { return 9; }
    [[nodiscard]] std::vector<TriageHotkeySlotData> const & sequenceSlots() const {
        return _data.sequence_slots;
    }
    [[nodiscard]] std::optional<TriageHotkeySlotData> sequenceSlot(int slot_index) const;
    bool setSequenceSlot(int slot_index, TriageHotkeySlotData const & slot);
    bool setSequenceSlotJson(int slot_index, std::optional<std::string> sequence_json);

signals:
    void pipelineChanged();
    void sequenceSlotsChanged();

private:
    static std::vector<TriageHotkeySlotData> makeDefaultSequenceSlots();
    void ensureSequenceSlotsInitialized();

    std::shared_ptr<DataManager> _data_manager;
    TriageSessionStateData _data;
};

#endif// TRIAGE_SESSION_STATE_HPP
