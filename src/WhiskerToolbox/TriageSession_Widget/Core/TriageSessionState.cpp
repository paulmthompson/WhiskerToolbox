/**
 * @file TriageSessionState.cpp
 * @brief Implementation of TriageSessionState
 */

#include "TriageSessionState.hpp"

#include <rfl/json.hpp>

#include <algorithm>
#include <iostream>

namespace {

bool isValidSlotIndex(int const slot_index) {
    return slot_index >= 1 && slot_index <= TriageSessionState::sequenceSlotCount();
}

int toVectorIndex(int const slot_index) {
    return slot_index - 1;
}

}// namespace

TriageSessionState::TriageSessionState(std::shared_ptr<DataManager> data_manager,
                                       QObject * parent)
    : EditorState(parent),
      _data_manager(std::move(data_manager)) {
    ensureSequenceSlotsInitialized();
}

QString TriageSessionState::getDisplayName() const {
    return QString::fromStdString(_data.display_name);
}

void TriageSessionState::setDisplayName(QString const & name) {
    auto const name_std = name.toStdString();
    if (_data.display_name != name_std) {
        _data.display_name = name_std;
        markDirty();
        emit displayNameChanged(name);
    }
}

std::string TriageSessionState::toJson() const {
    TriageSessionStateData data_to_serialize = _data;
    data_to_serialize.instance_id = getInstanceId().toStdString();
    return rfl::json::write(data_to_serialize);
}

bool TriageSessionState::fromJson(std::string const & json) {
    if (auto result = rfl::json::read<TriageSessionStateData>(json); result) {
        _data = *result;

        // Backward compatibility: migrate legacy single-pipeline payload into slot 1
        // only when slot payloads are absent/empty in the serialized state.
        if (_data.sequence_slots.empty() && _data.pipeline_json.has_value() &&
            !_data.pipeline_json->empty()) {
            _data.sequence_slots = makeDefaultSequenceSlots();
            _data.sequence_slots.front().sequence_json = _data.pipeline_json;
        }

        ensureSequenceSlotsInitialized();
        if (!_data.instance_id.empty()) {
            setInstanceId(QString::fromStdString(_data.instance_id));
        }
        emit stateChanged();
        return true;
    }
    std::cerr << "TriageSessionState::fromJson: Failed to parse JSON" << std::endl;
    return false;
}

void TriageSessionState::setPipelineJson(std::string_view const json) {
    std::string const json_str(json);
    if (_data.pipeline_json != json_str) {
        _data.pipeline_json = json_str;
        markDirty();
        emit pipelineChanged();
    }
}

void TriageSessionState::setTrackedRegionsKey(std::string_view const key) {
    std::string const key_str(key);
    if (_data.tracked_regions_key != key_str) {
        _data.tracked_regions_key = key_str;
        markDirty();
        emit stateChanged();
    }
}

std::optional<TriageHotkeySlotData> TriageSessionState::sequenceSlot(int const slot_index) const {
    if (!isValidSlotIndex(slot_index)) {
        return std::nullopt;
    }
    auto const index = toVectorIndex(slot_index);
    if (index < 0 || index >= static_cast<int>(_data.sequence_slots.size())) {
        return std::nullopt;
    }
    return _data.sequence_slots.at(static_cast<std::size_t>(index));
}

bool TriageSessionState::setSequenceSlot(int const slot_index,
                                         TriageHotkeySlotData const & slot) {
    if (!isValidSlotIndex(slot_index)) {
        return false;
    }

    ensureSequenceSlotsInitialized();
    auto updated = slot;
    updated.slot_index = slot_index;

    auto & existing = _data.sequence_slots.at(static_cast<std::size_t>(toVectorIndex(slot_index)));
    if (existing.slot_index == updated.slot_index &&
        existing.display_name == updated.display_name &&
        existing.enabled == updated.enabled &&
        existing.sequence_json == updated.sequence_json) {
        return true;
    }

    existing = std::move(updated);
    markDirty();
    emit sequenceSlotsChanged();
    emit stateChanged();
    return true;
}

bool TriageSessionState::setSequenceSlotJson(int const slot_index,
                                             std::optional<std::string> sequence_json) {
    if (!isValidSlotIndex(slot_index)) {
        return false;
    }

    ensureSequenceSlotsInitialized();
    auto & slot = _data.sequence_slots.at(static_cast<std::size_t>(toVectorIndex(slot_index)));
    if (slot.sequence_json == sequence_json) {
        return true;
    }

    slot.sequence_json = std::move(sequence_json);
    markDirty();
    emit sequenceSlotsChanged();
    emit stateChanged();
    return true;
}

std::vector<TriageHotkeySlotData> TriageSessionState::makeDefaultSequenceSlots() {
    std::vector<TriageHotkeySlotData> default_sequence_slots;
    default_sequence_slots.reserve(static_cast<std::size_t>(sequenceSlotCount()));
    for (int slot_index = 1; slot_index <= sequenceSlotCount(); ++slot_index) {
        TriageHotkeySlotData slot;
        slot.slot_index = slot_index;
        default_sequence_slots.push_back(std::move(slot));
    }
    return default_sequence_slots;
}

void TriageSessionState::ensureSequenceSlotsInitialized() {
    if (_data.sequence_slots.empty()) {
        _data.sequence_slots = makeDefaultSequenceSlots();
        return;
    }

    if (_data.sequence_slots.size() != static_cast<std::size_t>(sequenceSlotCount())) {
        auto normalized_slots = makeDefaultSequenceSlots();
        auto const copy_count = std::min(normalized_slots.size(), _data.sequence_slots.size());
        for (std::size_t i = 0; i < copy_count; ++i) {
            normalized_slots[i].display_name = _data.sequence_slots[i].display_name;
            normalized_slots[i].enabled = _data.sequence_slots[i].enabled;
            normalized_slots[i].sequence_json = _data.sequence_slots[i].sequence_json;
        }
        _data.sequence_slots = std::move(normalized_slots);
    }

    for (int slot_index = 1; slot_index <= sequenceSlotCount(); ++slot_index) {
        _data.sequence_slots[static_cast<std::size_t>(slot_index - 1)].slot_index = slot_index;
    }
}
