#ifndef TEST_FIXTURE_CONSTANTS_HPP
#define TEST_FIXTURE_CONSTANTS_HPP

#include <cstdint>

/**
 * @brief Common constants for test fixture builders
 * 
 * This header provides shared constants used across multiple builders
 * to avoid duplication and improve maintainability.
 */
namespace test_fixture_constants {

/**
 * @brief Default image width for spatial data (pixels)
 * 
 * This default is used for MaskData, LineData, and PointData when
 * no explicit image size is specified. Chosen to represent a typical
 * video resolution.
 */
constexpr uint32_t DEFAULT_IMAGE_WIDTH = 800;

/**
 * @brief Default image height for spatial data (pixels)
 * 
 * This default is used for MaskData, LineData, and PointData when
 * no explicit image size is specified. Chosen to represent a typical
 * video resolution.
 */
constexpr uint32_t DEFAULT_IMAGE_HEIGHT = 600;

} // namespace test_fixture_constants

#endif // TEST_FIXTURE_CONSTANTS_HPP
