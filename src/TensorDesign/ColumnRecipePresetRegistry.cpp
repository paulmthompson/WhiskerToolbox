/**
 * @file ColumnRecipePresetRegistry.cpp
 * @brief Built-in TensorDesign column recipe preset registry implementation.
 */

#include "ColumnRecipePresetRegistry.hpp"
#include "Presets/BuiltInPresets.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <string>

namespace Neuralyzer::TensorDesign {

namespace {

[[nodiscard]] bool modifierIdInList(
        IRowModifier const * modifier,
        std::vector<std::string> const & compatible_modifier_ids) {
    if (compatible_modifier_ids.empty()) {
        return true;
    }
    auto const modifier_id = modifier != nullptr ? modifier->id() : std::string{};
    return std::ranges::find(compatible_modifier_ids, modifier_id) != compatible_modifier_ids.end();
}

}// namespace

bool ColumnRecipePresetRegistry::registerPreset(std::unique_ptr<IColumnAggregator> aggregator) {
    auto const duplicate = std::ranges::any_of(
            _aggregators,
            [&aggregator](std::unique_ptr<IColumnAggregator> const & existing) {
                return existing->id() == aggregator->id();
            });
    if (duplicate) {
        return false;
    }
    _aggregators.push_back(std::move(aggregator));
    return true;
}

IColumnAggregator const * ColumnRecipePresetRegistry::find(std::string const & id) const {
    auto const iter = std::ranges::find_if(
            _aggregators,
            [&id](std::unique_ptr<IColumnAggregator> const & descriptor) {
                return descriptor->id() == id;
            });
    if (iter == _aggregators.end()) {
        return nullptr;
    }
    return iter->get();
}

std::optional<ColumnAggregatorExpansion> ColumnRecipePresetRegistry::expand(
        std::string const & id,
        ColumnRecipePresetArgs const & args) const {
    auto const * descriptor = find(id);
    if (descriptor == nullptr) {
        return std::nullopt;
    }
    return descriptor->expand(args);
}

std::optional<ColumnAggregatorExpansion> ColumnRecipePresetRegistry::expandJson(
        std::string const & id,
        nlohmann::json const & parameters) const {
    auto args = parseColumnRecipePresetArgs(parameters);
    if (!args.has_value()) {
        return std::nullopt;
    }
    return expand(id, args.value());
}

std::vector<IColumnAggregator const *> ColumnRecipePresetRegistry::descriptors() const {
    std::vector<IColumnAggregator const *> result;
    result.reserve(_aggregators.size());
    for (auto const & descriptor: _aggregators) {
        result.push_back(descriptor.get());
    }
    return result;
}

ColumnRecipePresetRegistry createBuiltInColumnRecipePresetRegistry() {
    ColumnRecipePresetRegistry registry;
    registry.registerPreset(std::make_unique<MeanValueAggregator>());
    registry.registerPreset(std::make_unique<EventCountAggregator>());
    registry.registerPreset(std::make_unique<EventPresenceAggregator>());
    registry.registerPreset(std::make_unique<AnalogSampleAggregator>());
    registry.registerPreset(std::make_unique<PointXyAggregator>());
    registry.registerPreset(std::make_unique<MultiPointXyAggregator>());
    registry.registerPreset(std::make_unique<TrialRelativeEventCountAggregator>());
    return registry;
}

std::optional<ColumnRecipePresetArgs> parseColumnRecipePresetArgs(nlohmann::json const & parameters) {
    if (!parameters.is_object()) {
        return std::nullopt;
    }

    ColumnRecipePresetArgs args;
    args.output_name = parameters.value("output_name", parameters.value("name", std::string{}));
    args.source_key = parameters.value("source_key", std::string{});
    args.binding_source_key = parameters.value("binding_source_key", std::string{});
    args.store_key = parameters.value("store_key", std::string{});
    args.name_prefix = parameters.value("name_prefix", std::string{});
    args.pre = parameters.value("pre", int64_t{0});
    args.post = parameters.value("post", int64_t{0});
    args.window_start = parameters.value("window_start", 0.0);
    args.window_end = parameters.value("window_end", 0.0);

    if (parameters.contains("source_keys")) {
        if (!parameters["source_keys"].is_array()) {
            return std::nullopt;
        }
        for (auto const & source_key: parameters["source_keys"]) {
            if (!source_key.is_string()) {
                return std::nullopt;
            }
            args.source_keys.push_back(source_key.get<std::string>());
        }
    }

    return args;
}

// --- RowModifierRegistry implementation ---

bool RowModifierRegistry::registerModifier(std::unique_ptr<IRowModifier> descriptor) {
    auto const duplicate = std::ranges::any_of(
            _modifiers,
            [&descriptor](std::unique_ptr<IRowModifier> const & existing) {
                return existing->id() == descriptor->id();
            });
    if (duplicate) {
        return false;
    }
    _modifiers.push_back(std::move(descriptor));
    return true;
}

IRowModifier const * RowModifierRegistry::find(std::string const & id) const {
    auto const iter = std::ranges::find_if(
            _modifiers,
            [&id](std::unique_ptr<IRowModifier> const & descriptor) {
                return descriptor->id() == id;
            });
    if (iter == _modifiers.end()) {
        return nullptr;
    }
    return iter->get();
}

std::vector<IRowModifier const *> RowModifierRegistry::descriptors() const {
    std::vector<IRowModifier const *> result;
    result.reserve(_modifiers.size());
    for (auto const & descriptor: _modifiers) {
        result.push_back(descriptor.get());
    }
    return result;
}

std::vector<IRowModifier const *> RowModifierRegistry::getModifiersFor(EffectiveRowType row_type) const {
    std::vector<IRowModifier const *> results;
    for (auto const & desc: _modifiers) {
        auto types = desc->supported_row_types();
        if (std::ranges::find(types, row_type) != types.end()) {
            results.push_back(desc.get());
        }
    }
    return results;
}

RowModifierRegistry createBuiltInRowModifierRegistry() {
    RowModifierRegistry registry;
    registry.registerModifier(std::make_unique<IntervalStartModifier>());
    registry.registerModifier(std::make_unique<WindowAroundIntervalStartModifier>());
    registry.registerModifier(std::make_unique<BindIntervalStartModifier>());
    return registry;
}

// --- ColumnAggregatorRegistry implementation ---

bool ColumnAggregatorRegistry::registerAggregator(std::unique_ptr<IColumnAggregator> descriptor) {
    auto const duplicate = std::ranges::any_of(
            _aggregators,
            [&descriptor](std::unique_ptr<IColumnAggregator> const & existing) {
                return existing->id() == descriptor->id();
            });
    if (duplicate) {
        return false;
    }
    _aggregators.push_back(std::move(descriptor));
    return true;
}

IColumnAggregator const * ColumnAggregatorRegistry::find(std::string const & id) const {
    auto const it = std::ranges::find_if(_aggregators, [&id](std::unique_ptr<IColumnAggregator> const & desc) {
        return desc->id() == id;
    });
    if (it != _aggregators.end()) {
        return it->get();
    }
    return nullptr;
}

std::vector<IColumnAggregator const *> ColumnAggregatorRegistry::descriptors() const {
    std::vector<IColumnAggregator const *> results;
    results.reserve(_aggregators.size());
    for (auto const & desc: _aggregators) {
        results.push_back(desc.get());
    }
    return results;
}

std::vector<IColumnAggregator const *> ColumnAggregatorRegistry::getAggregatorsFor(EffectiveRowType row_type) const {
    std::vector<IColumnAggregator const *> results;
    for (auto const & desc : _aggregators) {
        auto types = desc->supported_row_types();
        if (std::ranges::find(types, row_type) != types.end()) {
            results.push_back(desc.get());
        }
    }
    return results;
}

bool ColumnAggregatorRegistry::isCompatibleComposition(
        IRowModifier const * modifier,
        IColumnAggregator const & aggregator) {
    auto const rules = aggregator.composition_rules();

    if (rules.requires_pipeline_value_bindings) {
        if (modifier == nullptr || modifier->id() != "bind_interval_start") {
            return false;
        }
    }

    if (rules.required_row_geometry == EffectiveRowType::Interval && modifier != nullptr &&
        modifier->output_row_type() == EffectiveRowType::Timestamp) {
        return false;
    }

    if (rules.required_row_geometry == EffectiveRowType::Timestamp) {
        if (modifier != nullptr && modifier->output_row_type() != EffectiveRowType::Timestamp) {
            return false;
        }
    }

    if (!rules.compatible_modifier_ids.empty()) {
        auto const allows_no_modifier =
                std::ranges::find(rules.compatible_modifier_ids, std::string{}) !=
                rules.compatible_modifier_ids.end();
        if (modifier == nullptr) {
            if (!allows_no_modifier) {
                return false;
            }
        } else if (!modifierIdInList(modifier, rules.compatible_modifier_ids)) {
            return false;
        }
    }

    if (modifier != nullptr && modifier->output_row_type() == EffectiveRowType::Timestamp) {
        auto types = aggregator.supported_row_types();
        auto const supports_timestamp = std::ranges::find(
                                                types,
                                                EffectiveRowType::Timestamp) !=
                                        types.end();
        if (!supports_timestamp) {
            return false;
        }
    }

    return true;
}

std::vector<IColumnAggregator const *> ColumnAggregatorRegistry::getAggregatorsFor(
        AggregatorQueryContext const & ctx) const {
    std::vector<IColumnAggregator const *> results;
    for (auto const & desc: _aggregators) {
        if (ctx.tensor_column_only &&
            desc->composition_rules().output_kind != ColumnOutputKind::ScalarFloat) {
            continue;
        }
        auto types = desc->supported_row_types();
        if (std::ranges::find(types, ctx.effective_row_type) == types.end()) {
            continue;
        }
        if (!isCompatibleComposition(ctx.selected_modifier, *desc)) {
            continue;
        }
        results.push_back(desc.get());
    }
    return results;
}

ColumnAggregatorRegistry createBuiltInColumnAggregatorRegistry() {
    ColumnAggregatorRegistry registry;
    registry.registerAggregator(std::make_unique<MeanValueAggregator>());
    registry.registerAggregator(std::make_unique<EventCountAggregator>());
    registry.registerAggregator(std::make_unique<EventPresenceAggregator>());
    registry.registerAggregator(std::make_unique<AnalogSampleAggregator>());
    registry.registerAggregator(std::make_unique<PointXyAggregator>());
    registry.registerAggregator(std::make_unique<MultiPointXyAggregator>());
    registry.registerAggregator(std::make_unique<RasterEventsRelativeAggregator>());
    registry.registerAggregator(std::make_unique<TrialRelativeEventCountAggregator>());
    return registry;
}

}// namespace Neuralyzer::TensorDesign
