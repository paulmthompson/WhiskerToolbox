/**
 * @file ColumnRecipePresetRegistry.hpp
 * @brief Registry and built-in expansions for TensorDesign column recipe presets.
 */
#ifndef COLUMN_RECIPE_PRESET_REGISTRY_HPP
#define COLUMN_RECIPE_PRESET_REGISTRY_HPP

#include "ParameterSchema/ParameterSchema.hpp"
#include "TransformsV2/core/TensorColumnBuilders.hpp"

#include <nlohmann/json_fwd.hpp>

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace Neuralyzer::TensorDesign {

enum class ColumnRecipePresetSource : std::uint8_t {
    BuiltIn,
    User
};

struct ColumnRecipePresetArgs {
    std::string output_name;
    std::string source_key;
    std::string binding_source_key;
    std::string store_key;
    std::string name_prefix;
    std::vector<std::string> source_keys;
    int64_t pre = 0;
    int64_t post = 0;
    double window_start = 0.0;
    double window_end = 0.0;
};

struct ColumnRecipePresetExpansion {
    std::vector<TensorBuilders::ColumnRecipe> columns;
};

struct ColumnRecipePresetDescriptor {
    std::string id;
    std::string display_name;
    std::string description;
    ParameterSchema parameters;
    ColumnRecipePresetSource source = ColumnRecipePresetSource::BuiltIn;
    std::function<std::optional<ColumnRecipePresetExpansion>(ColumnRecipePresetArgs const &)> expand;
};

class ColumnRecipePresetRegistry {
public:
    bool registerPreset(ColumnRecipePresetDescriptor descriptor);

    [[nodiscard]] ColumnRecipePresetDescriptor const * find(std::string const & id) const;
    [[nodiscard]] std::optional<ColumnRecipePresetExpansion> expand(
            std::string const & id,
            ColumnRecipePresetArgs const & args) const;
    [[nodiscard]] std::optional<ColumnRecipePresetExpansion> expandJson(
            std::string const & id,
            nlohmann::json const & parameters) const;
    [[nodiscard]] std::vector<ColumnRecipePresetDescriptor const *> descriptors() const;

private:
    std::vector<ColumnRecipePresetDescriptor> _descriptors;
};

// Phase 9e: Decoupling Row Modifiers and Column Aggregators

enum class EffectiveRowType : std::uint8_t {
    Unchanged, ///< Does not change the row geometry (e.g., binds a value only)
    Interval,  ///< The row is a continuous interval
    Timestamp  ///< The row is an instantaneous point in time (Event or TimeFrame)
};

struct RowModifierExpansion {
    std::string row_pipeline_json;
    std::vector<TensorBuilders::PipelineValueBindingRecipe> pipeline_value_bindings;
};

struct RowModifierDescriptor {
    std::string id;
    std::string display_name;
    std::string description;
    EffectiveRowType output_row_type{EffectiveRowType::Unchanged};
    ParameterSchema parameters;
    ColumnRecipePresetSource source = ColumnRecipePresetSource::BuiltIn;
    std::function<std::optional<RowModifierExpansion>(ColumnRecipePresetArgs const &)> expand;
};

class RowModifierRegistry {
public:
    bool registerModifier(RowModifierDescriptor descriptor);
    [[nodiscard]] RowModifierDescriptor const * find(std::string const & id) const;
    [[nodiscard]] std::vector<RowModifierDescriptor const *> descriptors() const;
private:
    std::vector<RowModifierDescriptor> _descriptors;
};

struct ColumnAggregatorExpansion {
    std::vector<TensorBuilders::ColumnRecipe> columns;
};

struct ColumnAggregatorDescriptor {
    std::string id;
    std::string display_name;
    std::string description;
    std::vector<EffectiveRowType> supported_row_types;
    ParameterSchema parameters;
    ColumnRecipePresetSource source = ColumnRecipePresetSource::BuiltIn;
    std::function<std::optional<ColumnAggregatorExpansion>(ColumnRecipePresetArgs const &)> expand;
};

class ColumnAggregatorRegistry {
public:
    bool registerAggregator(ColumnAggregatorDescriptor descriptor);
    [[nodiscard]] ColumnAggregatorDescriptor const * find(std::string const & id) const;
    [[nodiscard]] std::vector<ColumnAggregatorDescriptor const *> descriptors() const;
    [[nodiscard]] std::vector<ColumnAggregatorDescriptor const *> getAggregatorsFor(EffectiveRowType row_type) const;
private:
    std::vector<ColumnAggregatorDescriptor> _descriptors;
};

[[nodiscard]] ColumnRecipePresetRegistry createBuiltInColumnRecipePresetRegistry();
[[nodiscard]] RowModifierRegistry createBuiltInRowModifierRegistry();
[[nodiscard]] ColumnAggregatorRegistry createBuiltInColumnAggregatorRegistry();

[[nodiscard]] std::optional<ColumnRecipePresetArgs> parseColumnRecipePresetArgs(
        nlohmann::json const & parameters);

}// namespace Neuralyzer::TensorDesign

#endif// COLUMN_RECIPE_PRESET_REGISTRY_HPP
