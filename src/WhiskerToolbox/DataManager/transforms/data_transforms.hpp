
#ifndef WHISKERTOOLBOX_DATA_TRANSFORMS_HPP
#define WHISKERTOOLBOX_DATA_TRANSFORMS_HPP

#include "ImageSize/ImageSize.hpp"

#include <memory>
#include <vector>

class AnalogTimeSeries;
class MaskData;
class MediaData;
class PointData;


std::shared_ptr<AnalogTimeSeries> area(const std::shared_ptr<MaskData>& mask_data);

void scale(std::shared_ptr<PointData>& point_data, ImageSize const& image_size_media);


#endif //WHISKERTOOLBOX_DATA_TRANSFORMS_HPP
