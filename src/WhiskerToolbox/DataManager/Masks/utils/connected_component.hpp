#ifndef WHISKERTOOLBOX_CONNECTED_COMPONENT_HPP
#define WHISKERTOOLBOX_CONNECTED_COMPONENT_HPP

#include "ImageSize/ImageSize.hpp"

#include <cstdint>
#include <vector>

std::vector<uint8_t> remove_small_clusters(std::vector<uint8_t> const & image, ImageSize image_size, int threshold);


#endif//WHISKERTOOLBOX_CONNECTED_COMPONENT_HPP
