#ifndef TENSORDESIGN_BUILTINPRESETS_HPP
#define TENSORDESIGN_BUILTINPRESETS_HPP

#include "PresetInterfaces.hpp"
#include <memory>

namespace Neuralyzer::TensorDesign {

// --- Row Modifiers ---

class IntervalStartModifier : public IRowModifier {
public:
    std::string id() const override;
    std::string display_name() const override;
    std::string description() const override;
    EffectiveRowType output_row_type() const override;
    std::vector<EffectiveRowType> supported_row_types() const override;
    ParameterSchema parameters() const override;
    std::optional<RowModifierExpansion> expand(ColumnRecipePresetArgs const & args) const override;
};

class WindowAroundIntervalStartModifier : public IRowModifier {
public:
    std::string id() const override;
    std::string display_name() const override;
    std::string description() const override;
    EffectiveRowType output_row_type() const override;
    std::vector<EffectiveRowType> supported_row_types() const override;
    ParameterSchema parameters() const override;
    std::optional<RowModifierExpansion> expand(ColumnRecipePresetArgs const & args) const override;
};

class BindIntervalStartModifier : public IRowModifier {
public:
    std::string id() const override;
    std::string display_name() const override;
    std::string description() const override;
    EffectiveRowType output_row_type() const override;
    std::vector<EffectiveRowType> supported_row_types() const override;
    ParameterSchema parameters() const override;
    std::optional<RowModifierExpansion> expand(ColumnRecipePresetArgs const & args) const override;
};

// --- Column Aggregators ---

class MeanValueAggregator : public IColumnAggregator {
public:
    std::string id() const override;
    std::string display_name() const override;
    std::string description() const override;
    std::vector<EffectiveRowType> supported_row_types() const override;
    AggregatorCompositionRules composition_rules() const override;
    ParameterSchema parameters() const override;
    std::optional<ColumnAggregatorExpansion> expand(ColumnRecipePresetArgs const & args) const override;
};

class EventCountAggregator : public IColumnAggregator {
public:
    std::string id() const override;
    std::string display_name() const override;
    std::string description() const override;
    std::vector<EffectiveRowType> supported_row_types() const override;
    AggregatorCompositionRules composition_rules() const override;
    ParameterSchema parameters() const override;
    std::optional<ColumnAggregatorExpansion> expand(ColumnRecipePresetArgs const & args) const override;
};

class EventPresenceAggregator : public IColumnAggregator {
public:
    std::string id() const override;
    std::string display_name() const override;
    std::string description() const override;
    std::vector<EffectiveRowType> supported_row_types() const override;
    AggregatorCompositionRules composition_rules() const override;
    ParameterSchema parameters() const override;
    std::optional<ColumnAggregatorExpansion> expand(ColumnRecipePresetArgs const & args) const override;
};

class AnalogSampleAggregator : public IColumnAggregator {
public:
    std::string id() const override;
    std::string display_name() const override;
    std::string description() const override;
    std::vector<EffectiveRowType> supported_row_types() const override;
    AggregatorCompositionRules composition_rules() const override;
    ParameterSchema parameters() const override;
    std::optional<ColumnAggregatorExpansion> expand(ColumnRecipePresetArgs const & args) const override;
};

class PointXyAggregator : public IColumnAggregator {
public:
    std::string id() const override;
    std::string display_name() const override;
    std::string description() const override;
    std::vector<EffectiveRowType> supported_row_types() const override;
    AggregatorCompositionRules composition_rules() const override;
    ParameterSchema parameters() const override;
    std::optional<ColumnAggregatorExpansion> expand(ColumnRecipePresetArgs const & args) const override;
};

class MultiPointXyAggregator : public IColumnAggregator {
public:
    std::string id() const override;
    std::string display_name() const override;
    std::string description() const override;
    std::vector<EffectiveRowType> supported_row_types() const override;
    AggregatorCompositionRules composition_rules() const override;
    ParameterSchema parameters() const override;
    std::optional<ColumnAggregatorExpansion> expand(ColumnRecipePresetArgs const & args) const override;
    std::vector<std::string> getDynamicOptions(std::string const& field_name, DataManager const * data_manager = nullptr) const override;
};

class RasterEventsRelativeAggregator : public IColumnAggregator {
public:
    std::string id() const override;
    std::string display_name() const override;
    std::string description() const override;
    std::vector<EffectiveRowType> supported_row_types() const override;
    AggregatorCompositionRules composition_rules() const override;
    ParameterSchema parameters() const override;
    std::optional<ColumnAggregatorExpansion> expand(ColumnRecipePresetArgs const & args) const override;
};

class TrialRelativeEventCountAggregator : public IColumnAggregator {
public:
    std::string id() const override;
    std::string display_name() const override;
    std::string description() const override;
    std::vector<EffectiveRowType> supported_row_types() const override;
    AggregatorCompositionRules composition_rules() const override;
    ParameterSchema parameters() const override;
    std::optional<ColumnAggregatorExpansion> expand(ColumnRecipePresetArgs const & args) const override;
};

} // namespace Neuralyzer::TensorDesign

#endif // TENSORDESIGN_BUILTINPRESETS_HPP
