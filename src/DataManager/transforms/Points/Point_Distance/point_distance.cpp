#include "point_distance.hpp"

#include "AnalogTimeSeries/Analog_Time_Series.hpp"
#include "Points/Point_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "transforms/utils/variant_type_check.hpp"
#include "transforms/v2/algorithms/PointDistance/PointDistance.hpp"

#include <iostream>
#include <map>

namespace {
    // Convert between enum types
    WhiskerToolbox::Transforms::V2::PointDistance::ReferenceType convertReferenceType(
        PointDistanceReferenceType type) {
        using V2Type = WhiskerToolbox::Transforms::V2::PointDistance::ReferenceType;
        switch (type) {
            case PointDistanceReferenceType::GlobalAverage:
                return V2Type::GlobalAverage;
            case PointDistanceReferenceType::RollingAverage:
                return V2Type::RollingAverage;
            case PointDistanceReferenceType::SetPoint:
                return V2Type::SetPoint;
            case PointDistanceReferenceType::OtherPointData:
                return V2Type::OtherPointData;
        }
        return V2Type::GlobalAverage;
    }
}

std::shared_ptr<AnalogTimeSeries> pointDistance(
    PointData const * point_data,
    PointDistanceReferenceType reference_type,
    int window_size,
    float reference_x,
    float reference_y,
    PointData const * reference_point_data) {

    if (!point_data) {
        return nullptr;
    }

    // Set up parameters for v2 transform
    WhiskerToolbox::Transforms::V2::PointDistance::PointDistanceParams params;
    params.reference_type = convertReferenceType(reference_type);
    params.window_size = window_size;
    params.reference_x = reference_x;
    params.reference_y = reference_y;

    // Call the v2 transform
    auto results = WhiskerToolbox::Transforms::V2::PointDistance::calculatePointDistance(
        *point_data, params, reference_point_data);

    // Convert results to AnalogTimeSeries
    std::map<int, float> distance_map;
    for (auto const & result : results) {
        distance_map[result.time] = result.distance;
    }

    return std::make_shared<AnalogTimeSeries>(distance_map);
}

std::shared_ptr<AnalogTimeSeries> pointDistance(
    PointData const * point_data,
    PointDistanceReferenceType reference_type,
    int window_size,
    float reference_x,
    float reference_y,
    PointData const * reference_point_data,
    ProgressCallback progressCallback) {

    (void)progressCallback; // TODO: Wire up progress callback to v2 context

    return pointDistance(point_data, reference_type, window_size,
                        reference_x, reference_y, reference_point_data);
}

///////////////////////////////////////////////////////////////////////////////

std::string PointDistanceOperation::getName() const {
    return "Calculate Point Distance";
}

std::type_index PointDistanceOperation::getTargetInputTypeIndex() const {
    return typeid(std::shared_ptr<PointData>);
}

std::unique_ptr<TransformParametersBase> PointDistanceOperation::getDefaultParameters() const {
    return std::make_unique<PointDistanceParameters>();
}

bool PointDistanceOperation::canApply(DataTypeVariant const & dataVariant) const {
    return canApplyToType<PointData>(dataVariant);
}

DataTypeVariant PointDistanceOperation::execute(DataTypeVariant const & dataVariant,
                                                 TransformParametersBase const * transformParameters,
                                                 ProgressCallback progressCallback) {

    // 1. Safely get pointer to the shared_ptr<PointData> if variant holds it.
    auto const * ptr_ptr = std::get_if<std::shared_ptr<PointData>>(&dataVariant);

    // 2. Validate the pointer from get_if and the contained shared_ptr.
    if (!ptr_ptr || !(*ptr_ptr)) {
        std::cerr << "PointDistanceOperation::execute called with incompatible variant type or null data." << std::endl;
        return {};
    }

    // 3. Get the parameters
    auto const * params = dynamic_cast<PointDistanceParameters const *>(transformParameters);
    if (!params) {
        std::cerr << "PointDistanceOperation::execute called with invalid parameters." << std::endl;
        return {};
    }

    // 4. Get the non-owning raw pointer to pass to the calculation function.
    PointData const * point_raw_ptr = (*ptr_ptr).get();

    // 5. Get reference point data if provided
    PointData const * ref_point_raw_ptr = nullptr;
    if (params->reference_point_data) {
        ref_point_raw_ptr = params->reference_point_data.get();
    }

    // 6. Call the core calculation logic.
    std::shared_ptr<AnalogTimeSeries> result_ts = pointDistance(
        point_raw_ptr,
        params->reference_type,
        params->window_size,
        params->reference_x,
        params->reference_y,
        ref_point_raw_ptr,
        progressCallback
    );

    // 7. Handle potential failure from the calculation function.
    if (!result_ts) {
        std::cerr << "PointDistanceOperation::execute: 'pointDistance' failed to produce a result." << std::endl;
        return {};
    }

    std::cout << "PointDistanceOperation executed successfully." << std::endl;
    return result_ts;
}

DataTypeVariant PointDistanceOperation::execute(DataTypeVariant const & dataVariant,
                                                 TransformParametersBase const * transformParameters) {
    return execute(dataVariant, transformParameters, nullptr);
}

