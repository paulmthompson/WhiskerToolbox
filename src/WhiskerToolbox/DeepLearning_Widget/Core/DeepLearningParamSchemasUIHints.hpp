/**
 * @file DeepLearningParamSchemasUIHints.hpp
 * @brief Forward declarations of `ParameterUIHints` explicit specializations for
 *        widget-level deep learning parameter structs.
 *
 * `extractParameterSchema<T>()` ends with `ParameterUIHints<T>::annotate(schema)`.
 * If an explicit specialization of `ParameterUIHints<T>` is only defined in a
 * `.cpp` file, other translation units that instantiate `extractParameterSchema<T>()`
 * never see that specialization and instantiate the primary template's empty
 * `annotate` instead. Include this header in any `.cpp` that calls
 * `extractParameterSchema` for `dl::widget::*` param types (and keep definitions
 * in `DeepLearningParamSchemas.cpp`).
 */

#ifndef DEEP_LEARNING_PARAM_SCHEMAS_UI_HINTS_HPP
#define DEEP_LEARNING_PARAM_SCHEMAS_UI_HINTS_HPP

#include "DeepLearningParamSchemas.hpp"

#include "ParameterSchema/ParameterSchema.hpp"

template<>
struct ParameterUIHints<dl::widget::DynamicInputSlotParams> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::widget::EncoderShapeParams> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::widget::OutputSlotParams> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::widget::PostEncoderSlotParams> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::widget::RecurrentBindingSlotParams> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::widget::RecurrentSequenceEntryParams> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::widget::RelativeCaptureParams> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::widget::SequenceEntryParams> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::widget::StaticCaptureInitParams> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::widget::StaticInputSlotParams> {
    static void annotate(ParameterSchema & schema);
};

template<>
struct ParameterUIHints<dl::widget::StaticSequenceEntryParams> {
    static void annotate(ParameterSchema & schema);
};

#endif// DEEP_LEARNING_PARAM_SCHEMAS_UI_HINTS_HPP
