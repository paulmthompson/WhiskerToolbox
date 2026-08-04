/**
 * @file ColumnRecipePresetRegistry.hpp
 * @brief Registry and built-in expansions for TensorDesign column recipe presets.
 */
#ifndef COLUMN_RECIPE_PRESET_REGISTRY_HPP
#define COLUMN_RECIPE_PRESET_REGISTRY_HPP

#include "ParameterSchema/ParameterSchema.hpp"
#include "TransformsV2/core/TensorColumnBuilders.hpp"
#include "Presets/PresetInterfaces.hpp"

#include <nlohmann/json_fwd.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Neuralyzer::TensorDesign {

class ColumnRecipePresetRegistry {
public:
    bool registerPreset(std::unique_ptr<IColumnAggregator> aggregator);

    [[nodiscard]] IColumnAggregator const * find(std::string const & id) const;
    [[nodiscard]] std::optional<ColumnAggregatorExpansion> expand(
            std::string const & id,
            ColumnRecipePresetArgs const & args) const;
    [[nodiscard]] std::optional<ColumnAggregatorExpansion> expandJson(
            std::string const & id,
            nlohmann::json const & parameters) const;
    [[nodiscard]] std::vector<IColumnAggregator const *> descriptors() const;

private:
    std::vector<std::unique_ptr<IColumnAggregator>> _aggregators;
};

// Phase 9e: Decoupling Row Modifiers and Column Aggregators

class RowModifierRegistry {
public:
    bool registerModifier(std::unique_ptr<IRowModifier> modifier);
    [[nodiscard]] IRowModifier const * find(std::string const & id) const;
    [[nodiscard]] std::vector<IRowModifier const *> descriptors() const;
    [[nodiscard]] std::vector<IRowModifier const *> getModifiersFor(EffectiveRowType row_type) const;

private:
    std::vector<std::unique_ptr<IRowModifier>> _modifiers;
};

class ColumnAggregatorRegistry {
public:
    bool registerAggregator(std::unique_ptr<IColumnAggregator> aggregator);
    [[nodiscard]] IColumnAggregator const * find(std::string const & id) const;
    [[nodiscard]] std::vector<IColumnAggregator const *> descriptors() const;
    [[nodiscard]] std::vector<IColumnAggregator const *> getAggregatorsFor(EffectiveRowType row_type) const;
    [[nodiscard]] std::vector<IColumnAggregator const *> getAggregatorsFor(
            AggregatorQueryContext const & ctx) const;
    [[nodiscard]] static bool isCompatibleComposition(
            IRowModifier const * modifier,
            IColumnAggregator const & aggregator);

private:
    std::vector<std::unique_ptr<IColumnAggregator>> _aggregators;
};

[[nodiscard]] ColumnRecipePresetRegistry createBuiltInColumnRecipePresetRegistry();
[[nodiscard]] RowModifierRegistry createBuiltInRowModifierRegistry();
[[nodiscard]] ColumnAggregatorRegistry createBuiltInColumnAggregatorRegistry();

[[nodiscard]] std::optional<ColumnRecipePresetArgs> parseColumnRecipePresetArgs(
        nlohmann::json const & parameters);

}// namespace Neuralyzer::TensorDesign

#endif// COLUMN_RECIPE_PRESET_REGISTRY_HPP
