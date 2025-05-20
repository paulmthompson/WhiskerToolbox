#ifndef WHISKERTOOLBOX_DATA_TRANSFORMS_HPP
#define WHISKERTOOLBOX_DATA_TRANSFORMS_HPP

#include "DataManager.hpp"
#include "ImageSize/ImageSize.hpp"

#include <memory>
#include <string>
#include <typeindex>
#include <functional>

class PointData;

void scale(std::shared_ptr<PointData> & point_data, ImageSize const & image_size_media);

class TransformParametersBase {
public:
    virtual ~TransformParametersBase() = default;
};

// Callback type for progress updates
using ProgressCallback = std::function<void(int progress)>;

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
                                   
    // New overload with progress reporting
    virtual DataTypeVariant execute(DataTypeVariant const & dataVariant,
                                   TransformParametersBase const * transformParameters,
                                   ProgressCallback progressCallback) {
        // Default implementation ignores progress callback
        return execute(dataVariant, transformParameters);
    }
};

#endif//WHISKERTOOLBOX_DATA_TRANSFORMS_HPP
