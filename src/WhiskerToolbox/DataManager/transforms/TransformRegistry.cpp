#include "TransformRegistry.hpp"

#include "transforms/AnalogTimeSeries/analog_event_threshold.hpp"
#include "transforms/AnalogTimeSeries/analog_hilbert_phase.hpp"
#include "transforms/AnalogTimeSeries/analog_interval_threshold.hpp"
#include "transforms/AnalogTimeSeries/analog_scaling.hpp"
#include "transforms/DigitalIntervalSeries/digital_interval_group.hpp"
#include "transforms/Lines/line_angle.hpp"
#include "transforms/Lines/line_clip.hpp"
#include "transforms/Lines/line_curvature.hpp"
#include "transforms/Lines/line_min_point_dist.hpp"
#include "transforms/Lines/line_point_extraction.hpp"
#include "transforms/Lines/line_resample.hpp"
#include "transforms/Lines/line_subsegment.hpp"
#include "transforms/Masks/mask_area.hpp"
#include "transforms/Masks/mask_skeletonize.hpp"
#include "transforms/Masks/mask_to_line.hpp"

#include <iostream>// For init messages
#include <map>
#include <memory>// unique_ptr
#include <string>
#include <typeindex>
#include <variant>// Needed for the public interface
#include <vector>


TransformRegistry::TransformRegistry() {

    std::cout << "Initializing Operation Registry..." << std::endl;

    _registerOperation(std::make_unique<MaskAreaOperation>());
    _registerOperation(std::make_unique<MaskToLineOperation>());
    _registerOperation(std::make_unique<MaskSkeletonizeOperation>());
    _registerOperation(std::make_unique<EventThresholdOperation>());
    _registerOperation(std::make_unique<IntervalThresholdOperation>());
    _registerOperation(std::make_unique<HilbertPhaseOperation>());
    _registerOperation(std::make_unique<AnalogScalingOperation>());
    _registerOperation(std::make_unique<LineAngleOperation>());
    _registerOperation(std::make_unique<LineMinPointDistOperation>());
    _registerOperation(std::make_unique<LineResampleOperation>());
    _registerOperation(std::make_unique<LineCurvatureOperation>());
    _registerOperation(std::make_unique<LineSubsegmentOperation>());
    _registerOperation(std::make_unique<LinePointExtractionOperation>());
    _registerOperation(std::make_unique<LineClipOperation>());
    _registerOperation(std::make_unique<GroupOperation>());

    _computeApplicableOperations();
    std::cout << "Operation Registry Initialized." << std::endl;
}

std::vector<std::string> TransformRegistry::getOperationNamesForVariant(DataTypeVariant const & dataVariant) const {
    // Get the type_index of the type currently stored in the variant.
    // Note: std::variant::type() returns the type_index of the *alternative*,
    // which in our case IS the std::shared_ptr<T>. This matches what
    // getTargetInputTypeIndex() should return.
    auto current_type_index = std::visit([](auto & v) -> std::type_index { return typeid(v); }, dataVariant);

    // Look up the type index in the pre-computed map
    auto it = type_index_to_op_names_.find(current_type_index);
    if (it != type_index_to_op_names_.end()) {
        return it->second;// Return the pre-computed list of names
    } else {
        // No registered operation targets this specific type_index.
        // This is normal if a data type has no operations defined for it.
        return {};// Return empty vector
    }
}

TransformOperation * TransformRegistry::findOperationByName(std::string const & operation_name) const {
    auto it = name_to_operation_.find(operation_name);
    if (it != name_to_operation_.end()) {
        return it->second;
    } else {
        // std::cerr << "Warning: Requested operation name not found: '" << operation_name << "'" << std::endl;
        return nullptr;
    }
}

void TransformRegistry::_registerOperation(std::unique_ptr<TransformOperation> op) {
    if (!op) return;
    std::string op_name = op->getName();
    if (name_to_operation_.count(op_name)) {
        std::cerr << "Warning: Operation with name '" << op_name << "' already registered." << std::endl;
        return;
    }
    std::cout << "Registering operation: " << op_name
              << " (Targets type index: " << op->getTargetInputTypeIndex().name() << ")"// Debug info
              << std::endl;
    name_to_operation_[op_name] = op.get();
    all_operations_.push_back(std::move(op));
}

void TransformRegistry::_computeApplicableOperations() {
    std::cout << "Computing applicable operations based on registered operations..." << std::endl;
    type_index_to_op_names_.clear();// Start fresh

    for (auto const & op_ptr: all_operations_) {
        if (!op_ptr) continue;
        // Get the type index this operation says it targets
        std::type_index target_type_index = op_ptr->getTargetInputTypeIndex();
        // Get the user-facing name of the operation
        std::string op_name = op_ptr->getName();

        // Add this operation's name to the list for its target type index
        type_index_to_op_names_[target_type_index].push_back(op_name);
    }

    std::cout << "Finished computing applicable operations." << std::endl;
// Debug print (optional): shows mangled names for type_index keys internally
#ifndef NDEBUG// Example: Only print in debug builds
    for (auto const & pair: type_index_to_op_names_) {
        std::cout << "  TypeIndex Hash(" << pair.first.hash_code()
                  << ", Name=" << pair.first.name() << ") supports: ";
        for (auto const & name: pair.second) std::cout << "'" << name << "' ";
        std::cout << std::endl;
    }
#endif
}
