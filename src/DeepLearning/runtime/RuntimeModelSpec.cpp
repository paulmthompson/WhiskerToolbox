#include "RuntimeModelSpec.hpp"

#include <rfl.hpp>
#include <rfl/json.hpp>

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>

namespace dl {

BatchMode BatchModeSpec::toBatchMode() const {
    if (recurrent_only.has_value() && recurrent_only.value()) {
        return RecurrentOnlyBatch{};
    }
    if (fixed.has_value()) {
        return FixedBatch{fixed.value()};
    }
    if (dynamic.has_value()) {
        return DynamicBatch{dynamic->min, dynamic->max};
    }
    // Default: unlimited dynamic
    return DynamicBatch{1, 0};
}

TensorSlotDescriptor SlotSpec::toDescriptor() const {
    TensorSlotDescriptor desc;
    desc.name = name;
    desc.shape = shape;
    desc.description = description.value_or("");
    desc.recommended_encoder = recommended_encoder.value_or("");
    desc.recommended_decoder = recommended_decoder.value_or("");
    desc.is_static = is_static.value_or(false);
    desc.is_boolean_mask = is_boolean_mask.value_or(false);
    desc.sequence_dim = sequence_dim.value_or(-1);
    return desc;
}

rfl::Result<RuntimeModelSpec>
RuntimeModelSpec::fromJson(std::string const & json_str) {
    return rfl::json::read<RuntimeModelSpec>(json_str);
}

rfl::Result<RuntimeModelSpec>
RuntimeModelSpec::fromJsonFile(std::filesystem::path const & path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return rfl::Error("Failed to open file: " + path.string());
    }

    std::ostringstream ss;
    ss << file.rdbuf();
    auto result = fromJson(ss.str());

    if (!result) {
        return result;
    }

    auto spec = std::move(result.value());

    // Resolve relative weights_path against the JSON file's directory
    if (spec.weights_path.has_value() && !spec.weights_path->empty()) {
        std::filesystem::path weights_p(spec.weights_path.value());
        if (weights_p.is_relative()) {
            auto resolved = path.parent_path() / weights_p;
            spec.weights_path = resolved.string();
        }
    }

    // Resolve relative weights_variants paths
    if (spec.weights_variants.has_value()) {
        for (auto & variant: spec.weights_variants.value()) {
            std::filesystem::path variant_p(variant.path);
            if (variant_p.is_relative()) {
                variant.path = (path.parent_path() / variant_p).string();
            }
        }
    }

    return spec;
}

std::string RuntimeModelSpec::toJson() const {
    return rfl::json::write(*this);
}

std::vector<TensorSlotDescriptor> RuntimeModelSpec::inputDescriptors() const {
    std::vector<TensorSlotDescriptor> descs;
    descs.reserve(inputs.size());
    for (auto const & slot: inputs) {
        descs.push_back(slot.toDescriptor());
    }
    return descs;
}

std::vector<TensorSlotDescriptor> RuntimeModelSpec::outputDescriptors() const {
    std::vector<TensorSlotDescriptor> descs;
    descs.reserve(outputs.size());
    for (auto const & slot: outputs) {
        descs.push_back(slot.toDescriptor());
    }
    return descs;
}

std::vector<std::string> RuntimeModelSpec::validate() const {
    std::vector<std::string> errors;

    if (model_id.empty()) {
        errors.emplace_back("model_id must not be empty");
    }

    if (display_name.empty()) {
        errors.emplace_back("display_name must not be empty");
    }

    // Check input slots
    std::set<std::string> input_names;
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        auto const & slot = inputs[i];
        if (slot.name.empty()) {
            errors.push_back("inputs[" + std::to_string(i) + "]: name must not be empty");
        } else if (!input_names.insert(slot.name).second) {
            errors.push_back("inputs[" + std::to_string(i) + "]: duplicate name '" + slot.name + "'");
        }

        if (slot.shape.empty()) {
            errors.push_back("inputs[" + std::to_string(i) + "]: shape must not be empty");
        }

        auto const seq_dim = slot.sequence_dim.value_or(-1);
        if (seq_dim >= 0 && seq_dim >= static_cast<int>(slot.shape.size())) {
            errors.push_back("inputs[" + std::to_string(i) + "]: sequence_dim (" +
                             std::to_string(seq_dim) + ") exceeds shape rank (" +
                             std::to_string(slot.shape.size()) + ")");
        }
    }

    // Check output slots
    std::set<std::string> output_names;
    for (std::size_t i = 0; i < outputs.size(); ++i) {
        auto const & slot = outputs[i];
        if (slot.name.empty()) {
            errors.push_back("outputs[" + std::to_string(i) + "]: name must not be empty");
        } else if (!output_names.insert(slot.name).second) {
            errors.push_back("outputs[" + std::to_string(i) + "]: duplicate name '" + slot.name + "'");
        }

        if (slot.shape.empty()) {
            errors.push_back("outputs[" + std::to_string(i) + "]: shape must not be empty");
        }
    }

    if (preferred_batch_size.has_value() && preferred_batch_size.value() < 0) {
        errors.emplace_back("preferred_batch_size must be >= 0");
    }

    if (max_batch_size.has_value() && max_batch_size.value() < 0) {
        errors.emplace_back("max_batch_size must be >= 0");
    }

    // Validate batch_mode
    if (batch_mode.has_value()) {
        auto const & bm = batch_mode.value();
        int modes_set = 0;
        if (bm.fixed.has_value()) ++modes_set;
        if (bm.dynamic.has_value()) ++modes_set;
        if (bm.recurrent_only.has_value() && bm.recurrent_only.value()) ++modes_set;

        if (modes_set > 1) {
            errors.emplace_back("batch_mode: only one of fixed, dynamic, or recurrent_only may be set");
        }
        if (bm.fixed.has_value() && bm.fixed.value() < 1) {
            errors.emplace_back("batch_mode.fixed must be >= 1");
        }
        if (bm.dynamic.has_value()) {
            if (bm.dynamic->min < 1) {
                errors.emplace_back("batch_mode.dynamic.min must be >= 1");
            }
            if (bm.dynamic->max > 0 && bm.dynamic->max < bm.dynamic->min) {
                errors.emplace_back("batch_mode.dynamic.max must be >= min (or 0 for unlimited)");
            }
        }
    }

    // Validate weights_variants
    if (weights_variants.has_value()) {
        std::set<int> batch_sizes_seen;
        for (std::size_t i = 0; i < weights_variants->size(); ++i) {
            auto const & variant = (*weights_variants)[i];
            if (variant.path.empty()) {
                errors.push_back("weights_variants[" + std::to_string(i) +
                                 "]: path must not be empty");
            }
            if (variant.batch_size < 1) {
                errors.push_back("weights_variants[" + std::to_string(i) +
                                 "]: batch_size must be >= 1");
            }
            if (!batch_sizes_seen.insert(variant.batch_size).second) {
                errors.push_back("weights_variants[" + std::to_string(i) +
                                 "]: duplicate batch_size " +
                                 std::to_string(variant.batch_size));
            }
        }
    }

    return errors;
}

}// namespace dl
