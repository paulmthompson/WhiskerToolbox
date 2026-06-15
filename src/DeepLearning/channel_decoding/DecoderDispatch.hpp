#ifndef WHISKERTOOLBOX_DECODER_DISPATCH_HPP
#define WHISKERTOOLBOX_DECODER_DISPATCH_HPP

/**
 * @file DecoderDispatch.hpp
 * @brief Type-safe dispatch for channel decoders (params variant → geometry).
 */

#include "ChannelDecoder.hpp"

#include "bindings/EncoderDecoderBindingTypes.hpp"

#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace at {
class Tensor;
}

namespace dl {

/**
 * @brief Geometry or feature-vector output from a channel decoder.
 */
using DecodedGeometryVariant = std::variant<
        Mask2D,
        Point2D<float>,
        Line2D,
        std::vector<float>>;

/**
 * @brief Whether decoder params require a 4D [B,C,H,W] input tensor.
 */
template<typename DecoderParams>
[[nodiscard]] constexpr bool isSpatialDecoderParams() {
    return std::is_same_v<DecoderParams, MaskDecoderParams> ||
           std::is_same_v<DecoderParams, PointDecoderParams> ||
           std::is_same_v<DecoderParams, LineDecoderParams>;
}

/**
 * @brief Whether the active decoder alternative requires a spatial 4D tensor.
 */
[[nodiscard]] bool isSpatialDecoder(DecoderVariant const & params);

/**
 * @brief Map decoder params type to DataManager data type name.
 *
 * @returns "MaskData", "PointData", "LineData", "TensorData", or "".
 */
template<typename DecoderParams>
[[nodiscard]] std::string dataTypeForDecoderParams();

/**
 * @brief Map active decoder params to DataManager data type name.
 */
[[nodiscard]] std::string dataTypeForDecoder(DecoderVariant const & params);

/**
 * @brief Map decoder params type to DecoderFactory registry name.
 */
template<typename DecoderParams>
[[nodiscard]] std::string decoderFactoryName();

/**
 * @brief Map active decoder params to DecoderFactory registry name.
 */
[[nodiscard]] std::string decoderFactoryName(DecoderVariant const & params);

/**
 * @brief Construct default decoder params from a factory registry name.
 *
 * @returns Params for the named decoder, or nullopt if @p factory_name is unknown.
 */
[[nodiscard]] std::optional<DecoderVariant> decoderParamsFromFactoryName(
        std::string const & factory_name);

/**
 * @brief Decode a model output tensor using typed decoder parameters.
 */
template<typename DecoderParams>
[[nodiscard]] DecodedGeometryVariant decodeToGeometry(
        at::Tensor const & tensor,
        DecoderContext const & ctx,
        DecoderParams const & params);

/**
 * @brief Decode a model output tensor using a decoder params variant.
 */
[[nodiscard]] DecodedGeometryVariant decodeToGeometry(
        at::Tensor const & tensor,
        DecoderContext const & ctx,
        DecoderVariant const & params);

}// namespace dl

#endif// WHISKERTOOLBOX_DECODER_DISPATCH_HPP
