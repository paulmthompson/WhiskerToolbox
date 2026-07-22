#ifndef NEURALYZER_DATA_TRANSFORMS_HPP
#define NEURALYZER_DATA_TRANSFORMS_HPP

#include "DataManagerTypes.hpp"

#include <memory>
#include <string>
#include <typeindex>
#include <functional>

class TransformParametersBase {
public:
    virtual ~TransformParametersBase() = default;
};

// Callback type for progress updates
using ProgressCallback = std::function<void(int progress)>;

/**
 * @brief Wraps a progress callback so the inner callback runs at most once per integer percent (0–100).
 *
 * Repeated calls with the same clamped percent are ignored. Useful at pipeline/UI boundaries where
 * transforms may report progress very frequently.
 *
 * @param inner Destination callback; if empty, returns an empty function.
 * @return Wrapper that forwards only when the clamped percent value changes; duplicate 100 values are
 *         collapsed to a single delivery.
 */
[[nodiscard]] ProgressCallback throttleProgressCallbackToWholePercents(ProgressCallback inner);

class TransformOperation {
public:
    virtual ~TransformOperation() = default;
    [[nodiscard]] virtual std::string getName() const = 0;

    [[nodiscard]] virtual std::type_index getTargetInputTypeIndex() const = 0;

    [[nodiscard]] virtual bool canApply(DataTypeVariant const & dataVariant) const = 0;

    [[nodiscard]] virtual std::unique_ptr<TransformParametersBase> getDefaultParameters() const {
        return nullptr;
    }

    virtual DataTypeVariant execute(DataTypeVariant const & dataVariant,
                                   TransformParametersBase const * transformParameters) = 0;
                                   

    virtual DataTypeVariant execute(DataTypeVariant const & dataVariant,
                                   TransformParametersBase const * transformParameters,
                                   ProgressCallback progressCallback) {
        static_cast<void>(progressCallback);
        return execute(dataVariant, transformParameters);
    }
};

#endif//NEURALYZER_DATA_TRANSFORMS_HPP
