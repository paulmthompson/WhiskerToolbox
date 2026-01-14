#include "Lineage/LineageRecorder.hpp"
#include "Entity/Lineage/LineageTypes.hpp"

#include <stdexcept>

namespace WhiskerToolbox::Entity::Lineage {

void LineageRecorder::record(
        LineageRegistry & registry,
        std::string const & output_key,
        std::string const & input_key,
        Transforms::V2::TransformLineageType lineage_type) {

    using LT = Transforms::V2::TransformLineageType;

    switch (lineage_type) {
        case LT::None:
            // No lineage tracking requested - don't record anything
            return;

        case LT::OneToOneByTime:
            registry.setLineage(output_key, OneToOneByTime{.source_key = input_key});
            break;

        case LT::AllToOneByTime:
            registry.setLineage(output_key, AllToOneByTime{.source_key = input_key});
            break;

        case LT::Subset:
            // For Subset, we need additional info about which entities
            // This should use recordSubset() instead
            throw std::invalid_argument(
                    "LineageRecorder::record(): Subset lineage requires explicit entity list. "
                    "Use recordSubset() instead.");

        case LT::Source:
            registry.setLineage(output_key, Source{});
            break;
    }
}

void LineageRecorder::recordMultiInput(
        LineageRegistry & registry,
        std::string const & output_key,
        std::vector<std::string> const & input_keys,
        Transforms::V2::TransformLineageType lineage_type) {

    using LT = Transforms::V2::TransformLineageType;

    if (input_keys.empty()) {
        throw std::invalid_argument("LineageRecorder::recordMultiInput(): input_keys cannot be empty");
    }

    // For multi-input, we use MultiSourceLineage
    switch (lineage_type) {
        case LT::None:
            // No lineage tracking requested
            return;

        case LT::OneToOneByTime:
            // Multi-input with 1:1 by time (like LineMinPointDist)
            registry.setLineage(output_key, MultiSourceLineage{
                                                    .source_keys = input_keys,
                                                    .strategy = MultiSourceLineage::CombineStrategy::ZipByTime});
            break;

        case LT::AllToOneByTime:
            // Multi-input reduction
            registry.setLineage(output_key, MultiSourceLineage{
                                                    .source_keys = input_keys,
                                                    .strategy = MultiSourceLineage::CombineStrategy::ZipByTime});
            break;

        case LT::Subset:
            throw std::invalid_argument(
                    "LineageRecorder::recordMultiInput(): Subset lineage not supported for multi-input");

        case LT::Source:
            // Source doesn't make sense for derived data with multiple inputs
            throw std::invalid_argument(
                    "LineageRecorder::recordMultiInput(): Source lineage not valid for derived data");
    }
}

void LineageRecorder::recordSource(
        LineageRegistry & registry,
        std::string const & data_key) {

    registry.setLineage(data_key, Source{});
}

}// namespace WhiskerToolbox::Entity::Lineage
