#ifndef TENSORDESIGN_PRESETINTERFACES_HPP
#define TENSORDESIGN_PRESETINTERFACES_HPP

#include "ParameterSchema/ParameterSchema.hpp"
#include "TransformsV2/core/TensorColumnBuilders.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class DataManager;


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

enum class EffectiveRowType : std::uint8_t {
    Unchanged,
    Interval,
    Timestamp
};

struct RowModifierExpansion {
    std::string row_pipeline_json;
    std::vector<TensorBuilders::PipelineValueBindingRecipe> pipeline_value_bindings;
};

class IRowModifier {
public:
    virtual ~IRowModifier() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual std::string display_name() const = 0;
    [[nodiscard]] virtual std::string description() const = 0;
    [[nodiscard]] virtual EffectiveRowType output_row_type() const = 0;
    [[nodiscard]] virtual std::vector<EffectiveRowType> supported_row_types() const = 0;
    [[nodiscard]] virtual ParameterSchema parameters() const = 0;
    [[nodiscard]] virtual ColumnRecipePresetSource source() const { return ColumnRecipePresetSource::BuiltIn; }

    [[nodiscard]] virtual std::optional<RowModifierExpansion> expand(ColumnRecipePresetArgs const & args) const = 0;

    [[nodiscard]] virtual std::vector<std::string> getDynamicOptions(std::string const& /*field_name*/, DataManager const * /*data_manager*/ = nullptr) const { return {}; }
};

struct ColumnAggregatorExpansion {
    std::vector<TensorBuilders::ColumnRecipe> columns;
};

enum class ColumnOutputKind : std::uint8_t {
    ScalarFloat,
    GatheredDataObject
};

enum class GatheredOutputType : std::uint8_t {
    Unspecified,
    DigitalEventSeries
};

struct AggregatorCompositionRules {
    ColumnOutputKind output_kind{ColumnOutputKind::ScalarFloat};
    GatheredOutputType gathered_output_type{GatheredOutputType::Unspecified};
    bool requires_pipeline_value_bindings{false};
    EffectiveRowType required_row_geometry{EffectiveRowType::Interval};
    std::vector<std::string> compatible_modifier_ids;
};

class IColumnAggregator {
public:
    virtual ~IColumnAggregator() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual std::string display_name() const = 0;
    [[nodiscard]] virtual std::string description() const = 0;
    [[nodiscard]] virtual std::vector<EffectiveRowType> supported_row_types() const = 0;
    [[nodiscard]] virtual AggregatorCompositionRules composition_rules() const = 0;
    [[nodiscard]] virtual ParameterSchema parameters() const = 0;
    [[nodiscard]] virtual ColumnRecipePresetSource source() const { return ColumnRecipePresetSource::BuiltIn; }

    [[nodiscard]] virtual std::optional<ColumnAggregatorExpansion> expand(ColumnRecipePresetArgs const & args) const = 0;

    [[nodiscard]] virtual std::vector<std::string> getDynamicOptions(std::string const& /*field_name*/, DataManager const * /*data_manager*/ = nullptr) const { return {}; }
};

struct AggregatorQueryContext {
    EffectiveRowType effective_row_type{EffectiveRowType::Interval};
    IRowModifier const * selected_modifier{nullptr};
    bool tensor_column_only{true};
};

} // namespace Neuralyzer::TensorDesign

#endif // TENSORDESIGN_PRESETINTERFACES_HPP
