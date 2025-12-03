
#include "TransformPipeline.hpp"

#include "DataManager/AnalogTimeSeries/Analog_Time_Series.hpp"
#include "DataManager/Lines/Line_Data.hpp"
#include "DataManager/Masks/Mask_Data.hpp"
#include "DataManager/Points/Point_Data.hpp"

namespace WhiskerToolbox::Transforms::V2 {

// ============================================================================
// Batch Variant Helpers
// ============================================================================

void pushToBatch(BatchVariant & batch, ElementVariant const & element) {
    std::visit([&](auto & vec, auto const & elem) {
        using VecT = std::decay_t<decltype(vec)>;
        using ElemT = std::decay_t<decltype(elem)>;
        // Check if vector value type matches element type
        if constexpr (std::is_same_v<typename VecT::value_type, ElemT>) {
            vec.push_back(elem);
        } else {
            // This should not happen in a correctly typed pipeline
            throw std::runtime_error("Type mismatch in pushToBatch: Vector and Element types do not match");
        }
    },
               batch, element);
}


BatchVariant initBatchFromElement(ElementVariant const & element) {
    return std::visit([](auto const & elem) -> BatchVariant {
        using ElemT = std::decay_t<decltype(elem)>;
        return std::vector<ElemT>{elem};
    },
                      element);
}

size_t getBatchSize(BatchVariant const & batch) {
    return std::visit([](auto const & vec) { return vec.size(); }, batch);
}

void clearBatch(BatchVariant & batch) {
    std::visit([](auto & vec) { vec.clear(); }, batch);
}

// ============================================================================
// Transform Pipeline
// ============================================================================

std::function<ElementVariant(ElementVariant)> TransformPipeline::buildTypeErasedFunction(
        PipelineStep const & step,
        TransformMetadata const * meta) const {
    auto & registry = ElementRegistry::instance();

    // Dispatch based on input and output types from metadata
    // This is where we handle the type erasure/recovery

    auto input_type = meta->input_type;
    auto output_type = meta->output_type;
    auto params_type = meta->params_type;

    // Build a lambda that captures the transform and its parameters
    // The lambda knows the concrete types and can do the variant conversions
    return buildTypeErasedFunctionWithParams(step, input_type, output_type, params_type);
}

std::function<ElementVariant(ElementVariant)> TransformPipeline::buildTypeErasedFunctionWithParams(
        PipelineStep const & step,
        std::type_index input_type,
        std::type_index output_type,
        std::type_index params_type) const {
    auto & registry = ElementRegistry::instance();

    // Capture a pointer to the step so we always access the current params
    // (which may have been modified by preprocessing)
    auto step_ptr = &step;
    return [&registry,
            name = step.transform_name,
            step_ptr,// Capture pointer to step to access mutable params
            input_type,
            output_type,
            params_type](ElementVariant input) -> ElementVariant {
        return registry.executeWithDynamicParams(
                name, input, step_ptr->params, input_type, output_type, params_type);
    };
}

DataTypeVariant executePipeline(DataTypeVariant const & input, TransformPipeline const & pipeline) {
    return std::visit([&](auto const & ptr) -> DataTypeVariant {
        using T = typename std::remove_reference_t<decltype(*ptr)>;
        // Check if T is a valid input container (has DataTraits)
        if constexpr (TypeTraits::HasDataTraits<T>) {
            return pipeline.execute<T>(*ptr);
        } else {
            throw std::runtime_error("Unsupported input container type in variant");
        }
    },
                      input);
}

template<typename InputContainer>
DataTypeVariant TransformPipeline::execute(InputContainer const & input) const {
    if (steps_.empty()) {
        throw std::runtime_error("Pipeline has no steps");
    }

    auto & registry = ElementRegistry::instance();

    // 1. Compile pipeline into segments
    std::vector<Segment> segments;
    bool is_ragged = InputContainer::DataTraits::is_ragged;

    for (size_t i = 0; i < steps_.size(); ++i) {
        auto const & step = steps_[i];
        auto const * meta = registry.getMetadata(step.transform_name);
        if (!meta) throw std::runtime_error("Transform not found: " + step.transform_name);

        if (meta->is_time_grouped) {
            // Time-grouped transform is always its own segment
            Segment seg;
            seg.is_element_wise = false;
            seg.step_indices = {i};
            seg.input_type = meta->input_type;
            seg.output_type = meta->output_type;
            segments.push_back(std::move(seg));

            // Assume time-grouped transforms produce ragged output unless proven otherwise
            if (meta->produces_single_output) {
                is_ragged = false;
            } else {
                is_ragged = true;
            }
        } else {
            // Element-wise transform: merge with previous if possible
            if (!segments.empty() && segments.back().is_element_wise) {
                segments.back().step_indices.push_back(i);
                segments.back().output_type = meta->output_type;
            } else {
                Segment seg;
                seg.is_element_wise = true;
                seg.step_indices = {i};
                seg.input_type = meta->input_type;
                seg.output_type = meta->output_type;
                segments.push_back(std::move(seg));
            }
            // Element-wise preserves raggedness
        }
    }

    // 2. Build fused functions for element segments
    for (auto & seg: segments) {
        if (seg.is_element_wise) {
            std::vector<std::function<ElementVariant(ElementVariant)>> chain;
            for (size_t idx: seg.step_indices) {
                auto const & step = steps_[idx];
                auto const * meta = registry.getMetadata(step.transform_name);
                chain.push_back(buildTypeErasedFunction(step, meta));
            }

            seg.fused_fn = [chain = std::move(chain)](ElementVariant input) -> ElementVariant {
                ElementVariant current = std::move(input);
                for (auto const & fn: chain) {
                    current = fn(std::move(current));
                }
                return current;
            };
        }
    }

    // 3. Determine output container type and dispatch
    std::type_index final_type = segments.back().output_type;

    if (final_type == typeid(float)) {
        if (is_ragged) {
            return executeImpl<InputContainer, RaggedAnalogTimeSeries>(input, segments);
        } else {
            return executeImpl<InputContainer, AnalogTimeSeries>(input, segments);
        }
    } else if (final_type == typeid(Mask2D)) {
        return executeImpl<InputContainer, MaskData>(input, segments);
    } else if (final_type == typeid(Line2D)) {
        return executeImpl<InputContainer, LineData>(input, segments);
    } else if (final_type == typeid(Point2D<float>)) {
        return executeImpl<InputContainer, PointData>(input, segments);
    } else {
        throw std::runtime_error("Unsupported output element type: " + std::string(final_type.name()));
    }
}

template DataTypeVariant TransformPipeline::execute<RaggedAnalogTimeSeries>(RaggedAnalogTimeSeries const &) const;
template DataTypeVariant TransformPipeline::execute<AnalogTimeSeries>(AnalogTimeSeries const &) const;
template DataTypeVariant TransformPipeline::execute<MaskData>(MaskData const &) const;
template DataTypeVariant TransformPipeline::execute<LineData>(LineData const &) const;
template DataTypeVariant TransformPipeline::execute<PointData>(PointData const &) const;

}// namespace WhiskerToolbox::Transforms::V2