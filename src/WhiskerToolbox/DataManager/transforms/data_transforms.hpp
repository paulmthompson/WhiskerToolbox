
#ifndef WHISKERTOOLBOX_DATA_TRANSFORMS_HPP
#define WHISKERTOOLBOX_DATA_TRANSFORMS_HPP

#include "DataManager.hpp"
#include "ImageSize/ImageSize.hpp"

#include <any>
#include <memory>
#include <string>

class PointData;

void scale(std::shared_ptr<PointData> & point_data, ImageSize const & image_size_media);

class TransformOperation {
public:
    virtual ~TransformOperation() = default;
    [[nodiscard]] virtual std::string getName() const = 0;

    [[nodiscard]] virtual bool canApply(DataTypeVariant const & dataVariant) const = 0;

    virtual std::any execute(DataTypeVariant const & dataVariant) = 0;
};

#endif//WHISKERTOOLBOX_DATA_TRANSFORMS_HPP
