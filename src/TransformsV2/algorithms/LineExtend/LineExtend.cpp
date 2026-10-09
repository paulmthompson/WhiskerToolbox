/// @file LineExtend.cpp
/// @brief V2 binary element transform that extends a line to a reference polyline.

#include "LineExtend.hpp"

#include "CoreGeometry/line_geometry.hpp"
#include "core/ComputeContext.hpp"

namespace Neuralyzer::Transforms::V2::Examples {

Line2D extendLineAtReference(
        Line2D const & line,
        Line2D const & reference_line,
        LineExtendParams const & params) {

    return extend_line_at_reference(
            line,
            reference_line,
            params.extend_end,
            params.tangent_distance_pixels);
}

Line2D extendLineAtReferenceWithContext(
        Line2D const & line,
        Line2D const & reference_line,
        LineExtendParams const & params,
        [[maybe_unused]] ComputeContext const & ctx) {

    return extendLineAtReference(line, reference_line, params);
}

}// namespace Neuralyzer::Transforms::V2::Examples
