/**
 * @file ContainerTypeIndex.test.cpp
 * @brief Unit tests for container/element type mapping utilities.
 */

#include "DataManager/utils/ContainerElementMapping.hpp"
#include "DataManager/utils/ContainerTypeIndex.hpp"
#include "DataManager/utils/DataTypeIndexBridge.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"

#include <catch2/catch_test_macros.hpp>

#include <type_traits>

using namespace WhiskerToolbox::TypeTraits;

static_assert(std::is_same_v<ElementFor_t<MaskData>, Mask2D>);
static_assert(std::is_same_v<ContainerFor_t<Mask2D>, MaskData>);
static_assert(std::is_same_v<ContainerFor_t<float>, RaggedAnalogTimeSeries>);

TEST_CASE("TypeIndexMapper resolves container types", "[datamanager][container_type_index]") {
    CHECK_NOTHROW(TypeIndexMapper::stringToContainer("MaskData"));
    CHECK_NOTHROW(TypeIndexMapper::stringToContainer("mask"));
    CHECK_NOTHROW(TypeIndexMapper::stringToContainer("RaggedAnalogTimeSeries"));
}

TEST_CASE("TypeIndexMapper round-trips container and element", "[datamanager][container_type_index]") {
    auto const container = TypeIndexMapper::stringToContainer("MaskData");
    auto const element = TypeIndexMapper::containerToElement(container);
    auto const back = TypeIndexMapper::elementToContainer(element);
    CHECK(back == container);
}

TEST_CASE("TypeIndexMapper reports ragged containers", "[datamanager][container_type_index]") {
    CHECK(TypeIndexMapper::isContainerRagged(typeid(MaskData)));
    CHECK(TypeIndexMapper::isContainerRagged(typeid(RaggedAnalogTimeSeries)));
    CHECK_FALSE(TypeIndexMapper::isContainerRagged(typeid(AnalogTimeSeries)));
}

TEST_CASE("dmDataTypeToContainerTypeIndex bridges DM_DataType", "[datamanager][container_type_index]") {
    auto const mask_index = dmDataTypeToContainerTypeIndex(DM_DataType::Mask);
    CHECK(mask_index == typeid(MaskData));

    auto const dm_type = containerTypeIndexToDmDataType(mask_index);
    REQUIRE(dm_type.has_value());
    CHECK(*dm_type == DM_DataType::Mask);
}
