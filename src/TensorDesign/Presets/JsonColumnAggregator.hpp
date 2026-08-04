#ifndef TENSORDESIGN_JSONCOLUMNPRESET_HPP
#define TENSORDESIGN_JSONCOLUMNPRESET_HPP

#include "PresetInterfaces.hpp"
#include "Presets/JsonPresetDto.hpp"
#include <regex>
#include <sstream>

namespace Neuralyzer::TensorDesign {

class JsonColumnAggregator : public IColumnAggregator {
public:
    explicit JsonColumnAggregator(JsonColumnAggregatorPresetDto data);

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] std::string display_name() const override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] std::vector<EffectiveRowType> supported_row_types() const override;
    [[nodiscard]] AggregatorCompositionRules composition_rules() const override;
    [[nodiscard]] ParameterSchema parameters() const override;
    [[nodiscard]] ColumnRecipePresetSource source() const override { return ColumnRecipePresetSource::User; }

    [[nodiscard]] std::optional<ColumnAggregatorExpansion> expand(ColumnRecipePresetArgs const & args) const override;

private:
    JsonColumnAggregatorPresetDto _data;
    std::vector<EffectiveRowType> _supported_row_types;
    AggregatorCompositionRules _composition_rules;
};

} // namespace Neuralyzer::TensorDesign

#endif // TENSORDESIGN_JSONCOLUMNPRESET_HPP
