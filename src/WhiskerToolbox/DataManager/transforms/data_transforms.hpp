
#ifndef WHISKERTOOLBOX_DATA_TRANSFORMS_HPP
#define WHISKERTOOLBOX_DATA_TRANSFORMS_HPP

#include "ImageSize/ImageSize.hpp"

#include <memory>

class PointData;

void scale(std::shared_ptr<PointData> & point_data, ImageSize const & image_size_media);


#endif//WHISKERTOOLBOX_DATA_TRANSFORMS_HPP
