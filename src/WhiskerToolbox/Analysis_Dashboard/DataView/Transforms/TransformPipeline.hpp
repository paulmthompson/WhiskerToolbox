#ifndef DATAVIEW_TRANSFORMPIPELINE_HPP
#define DATAVIEW_TRANSFORMPIPELINE_HPP

#include "Analysis_Dashboard/DataView/Transforms/IDataViewTransform.hpp"

#include <memory>
#include <vector>
#include <functional>

namespace AnalysisDashboard {

/**
 * @brief Ordered list of transforms with default evaluation
 */
class TransformPipeline {
public:
    void clear() { _fns.clear(); }

    void addTransform(std::unique_ptr<IDataViewTransform> t) {
        if (!t) return;
        // Capture as shared_ptr to ensure stable lifetime inside callable
        std::shared_ptr<IDataViewTransform> sp(std::move(t));
        _fns.emplace_back([sp](DataViewContext const & c, DataViewState & s){ return sp->apply(c, s); });
    }

    DataViewState evaluate(DataViewContext const & ctx) const {
        DataViewState state = makeDefaultDataViewState(ctx.rowCount);
        for (auto const & fn : _fns) {
            if (!fn) continue;
            bool ok = fn(ctx, state);
            if (!ok) {
                // On failure, continue with current state; errors are expected to be logged inside transforms
                continue;
            }
        }
        return state;
    }

private:
    std::vector<std::function<bool(DataViewContext const &, DataViewState &)>> _fns;
};

} // namespace AnalysisDashboard

#endif // DATAVIEW_TRANSFORMPIPELINE_HPP

