#ifndef SKELETONIZE_HPP
#define SKELETONIZE_HPP

#include <cstdint>
#include <vector>

std::vector<uint8_t> fast_skeletonize(std::vector<uint8_t> const & image, size_t height, size_t width);


#endif// SKELETONIZE_HPP
