#ifndef WHISKERTOOLBOX_ENCODER_DISPATCH_HPP
#define WHISKERTOOLBOX_ENCODER_DISPATCH_HPP

/**
 * @file EncoderDispatch.hpp
 * @brief Type-safe dispatch for channel encoders (params variant → tensor).
 */

#include "ChannelEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"

#include <cstdint>
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
 * @brief User-configurable encoder parameters for all registered channel encoders.
 */
using EncoderParamsVariant = std::variant<
        ImageEncoderParams,
        Point2DEncoderParams,
        Mask2DEncoderParams,
        Line2DEncoderParams>;

/**
 * @brief Pixel data for ImageEncoder::encode (uint8 or float32).
 */
struct ImageEncodingSource {
    ImageSize size;
    int channels = 1;
    bool is_8bit = true;
    std::vector<uint8_t> data_u8;
    std::vector<float> data_f32;
};

/**
 * @brief Geometry or image source data paired with encoder params at encode time.
 */
using EncodingSourceVariant = std::variant<
        ImageEncodingSource,
        std::vector<Point2D<float>>,
        Mask2D,
        Line2D>;

/**
 * @brief Whether encoder params target MediaData (image) input.
 */
template<typename EncoderParams>
[[nodiscard]] constexpr bool isImageEncoderParams() {
    return std::is_same_v<EncoderParams, ImageEncoderParams>;
}

/**
 * @brief Whether the active encoder alternative targets MediaData.
 */
[[nodiscard]] bool isImageEncoder(EncoderParamsVariant const & params);

/**
 * @brief Map encoder params type to DataManager data type name.
 *
 * @returns "MediaData", "PointData", "MaskData", "LineData", or "".
 */
template<typename EncoderParams>
[[nodiscard]] std::string dataTypeForEncoderParams();

/**
 * @brief Map active encoder params to DataManager data type name.
 */
[[nodiscard]] std::string dataTypeForEncoder(EncoderParamsVariant const & params);

/**
 * @brief Map encoder params type to EncoderFactory registry name.
 */
template<typename EncoderParams>
[[nodiscard]] std::string encoderFactoryName();

/**
 * @brief Map active encoder params to EncoderFactory registry name.
 */
[[nodiscard]] std::string encoderFactoryName(EncoderParamsVariant const & params);

/**
 * @brief Construct default encoder params from a factory registry name.
 *
 * @returns Params for the named encoder, or nullopt if @p factory_name is unknown.
 */
[[nodiscard]] std::optional<EncoderParamsVariant> encoderParamsFromFactoryName(
        std::string const & factory_name);

/**
 * @brief Encode pre-fetched source data into a tensor channel using typed params.
 *
 * @param source_image_size Original image dimensions for geometry coordinate scaling.
 *        When width/height are non-positive, tensor (ctx.height, ctx.width) is used.
 */
template<typename EncoderParams>
void encodeToTensor(
        EncodingSourceVariant const & source,
        at::Tensor & tensor,
        EncoderContext const & ctx,
        ImageSize source_image_size,
        EncoderParams const & params);

/**
 * @brief Encode pre-fetched source data using an encoder params variant.
 */
void encodeToTensor(
        EncodingSourceVariant const & source,
        at::Tensor & tensor,
        EncoderContext const & ctx,
        ImageSize source_image_size,
        EncoderParamsVariant const & params);

}// namespace dl

#endif// WHISKERTOOLBOX_ENCODER_DISPATCH_HPP
