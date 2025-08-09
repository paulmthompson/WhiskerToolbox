#ifndef DATAVIEW_TRANSFORMPIPELINE_HPP
#define DATAVIEW_TRANSFORMPIPELINE_HPP

#include "Analysis_Dashboard/DataView/Transforms/IDataViewTransform.hpp"

#include <memory>
#include <vector>

/**
 * @brief Ordered list of transforms with default evaluation
 */
class TransformPipeline {
public:
    void clear() { _transforms.clear(); }

    void addTransform(std::unique_ptr<IDataViewTransform> t) {
        _transforms.push_back(std::move(t));
    }

    DataViewState evaluate(DataViewContext const & ctx) const {
        DataViewState state = makeDefaultDataViewState(ctx.rowCount);
        for (auto const & t : _transforms) {
            if (!t) continue;
            bool ok = t->apply(ctx, state);
            if (!ok) {
                // On failure, continue with current state; errors are expected to be logged inside transforms
                continue;
            }
        }
        return state;
    }

private:
    std::vector<std::unique_ptr<IDataViewTransform>> _transforms;
};

#endif // DATAVIEW_TRANSFORMPIPELINE_HPP

