#ifndef DATAVIEW_COLORBYFEATURETRANSFORM_HPP
#define DATAVIEW_COLORBYFEATURETRANSFORM_HPP

#include "Analysis_Dashboard/DataView/Transforms/IDataViewTransform.hpp"
#include "Analysis_Dashboard/DataView/Transforms/ColumnAccessor.hpp"

#include <unordered_map>

class ColorByFeatureTransform final : public IDataViewTransform {
public:
    enum class Mode { DiscreteBool, DiscreteInt, ContinuousFloat };

    QString tableId;
    QString columnName;
    Mode mode{Mode::DiscreteBool};

    // Discrete mappings: value -> palette index [0..31]
    std::unordered_map<int, uint32_t> discreteMap; // for bool or int

    // Continuous mapping parameters
    double minValue{0.0};
    double maxValue{1.0};

    QString id() const override { return "ColorByFeature"; }
    QString displayName() const override { return "Color By Feature"; }

    bool apply(DataViewContext const & context, DataViewState & state) override {
        if (tableId.isEmpty() || columnName.isEmpty()) return true;
        auto vals = DataViewColumns::loadScalarAsDoubles(context.tableRegistry, tableId, columnName, context.rowCount);
        if (!vals || vals->size() != context.rowCount) return false;

        if (!state.rowColorIndices) state.rowColorIndices = std::vector<uint32_t>(context.rowCount, 0);
        auto & colorIdx = *state.rowColorIndices;

        switch (mode) {
            case Mode::DiscreteBool:
            case Mode::DiscreteInt: {
                // Map double->int key
                for (size_t i = 0; i < vals->size(); ++i) {
                    int key = static_cast<int>((*vals)[i]);
                    auto it = discreteMap.find(key);
                    colorIdx[i] = (it != discreteMap.end()) ? it->second : 0u;
                }
                break;
            }
            case Mode::ContinuousFloat: {
                double range = (maxValue > minValue) ? (maxValue - minValue) : 1.0;
                for (size_t i = 0; i < vals->size(); ++i) {
                    double t = ((*vals)[i] - minValue) / range; // 0..1
                    if (std::isnan(t)) { colorIdx[i] = 0u; continue; }
                    t = std::clamp(t, 0.0, 1.0);
                    // Map 0..1 to palette indices 1..31 (0 reserved as base color)
                    uint32_t idx = 1u + static_cast<uint32_t>(t * 30.0);
                    colorIdx[i] = std::min(idx, 31u);
                }
                break;
            }
        }
        return true;
    }
};

#endif // DATAVIEW_COLORBYFEATURETRANSFORM_HPP

