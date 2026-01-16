#ifndef WHISKERTOOLBOX_LINEAGE_RECORDER_HPP
#define WHISKERTOOLBOX_LINEAGE_RECORDER_HPP

#include "Entity/Lineage/LineageRegistry.hpp"
#include "Entity/Lineage/LineageTypes.hpp"
#include "transforms/v2/extension/TransformTypes.hpp"

#include <stdexcept>
#include <string>

namespace WhiskerToolbox::Entity::Lineage {

/**
 * @brief Helper to record lineage from v2 transform pipelines
 * 
 * Translates from the transform-centric TransformLineageType (which describes
 * what a transform does) to the storage-centric Lineage::Descriptor (which 
 * describes how to resolve source entities).
 * 
 * @example
 * @code
 * // After executing a pipeline
 * auto result = pipeline.execute<MaskData, AnalogTimeSeries>(input);
 * 
 * // Store result in DataManager
 * dm->setData("mask_areas", result);
 * 
 * // Record lineage
 * LineageRecorder::record(
 *     *dm->getLineageRegistry(),
 *     "mask_areas",               // output key
 *     "masks",                    // input key
 *     TransformLineageType::OneToOneByTime
 * );
 * @endcode
 */
class LineageRecorder {
public:
    /**
     * @brief Record lineage for a derived container
     * 
     * Converts TransformLineageType to appropriate Lineage::Descriptor and
     * registers it in the LineageRegistry.
     * 
     * @param registry The LineageRegistry to record to
     * @param output_key Key of the derived (output) container
     * @param input_key Key of the source (input) container
     * @param lineage_type The transform's lineage type
     * 
     * @throws std::invalid_argument if lineage_type is None or requires more info
     */
    static void record(
            LineageRegistry & registry,
            std::string const & output_key,
            std::string const & input_key,
            Transforms::V2::TransformLineageType lineage_type);

    /**
     * @brief Record lineage for multi-input transforms
     * 
     * For transforms like LineMinPointDist that take multiple inputs.
     * 
     * @param registry The LineageRegistry to record to
     * @param output_key Key of the derived (output) container
     * @param input_keys Keys of the source (input) containers
     * @param lineage_type The transform's lineage type (typically OneToOneByTime)
     */
    static void recordMultiInput(
            LineageRegistry & registry,
            std::string const & output_key,
            std::vector<std::string> const & input_keys,
            Transforms::V2::TransformLineageType lineage_type);

    /**
     * @brief Record source lineage (for original/loaded data)
     * 
     * Marks a container as source data with no parent dependencies.
     * 
     * @param registry The LineageRegistry to record to
     * @param data_key Key of the source container
     */
    static void recordSource(
            LineageRegistry & registry,
            std::string const & data_key);
};

}// namespace WhiskerToolbox::Entity::Lineage

#endif// WHISKERTOOLBOX_LINEAGE_RECORDER_HPP
