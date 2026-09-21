#ifndef NEURALYZER_DECODER_FACTORY_HPP
#define NEURALYZER_DECODER_FACTORY_HPP

/**
 * @file DecoderFactory.hpp
 * @brief Factory for channel decoders that map model output tensors back into
 *        DataManager geometry types.
 *
 * @par Spatial scaling contract
 * Every decoder registered here must upsample or reproject from tensor spatial
 * dimensions (`DecoderContext::height` / `DecoderContext::width`) to
 * `DecoderContext::target_image_size` using the **same convention** as the
 * paired encoders in @ref EncoderFactory.
 *
 * - **Target convention:** pixel-center grid (`align_corners=false`), matching
 *   PyTorch `ImageEncoder` resize and OpenCV `cv2.resize` sampling geometry.
 * - **Continuous geometry** (points, lines): inverse of the encoder center mapping.
 * - **Discrete masks:** inverse nearest-neighbor raster expansion (see
 *   `resize_mask` in `mask_utils.hpp`), not independent per-point scaling.
 *
 * Encode and decode must be paired inverses per data category (continuous float
 * vs discrete mask raster). Do not introduce ad-hoc scaling in individual
 * decoders.
 *
 * @see EncoderFactory
 * @see DecoderDispatch
 */

#include "ChannelDecoder.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dl {

/**
 * @brief Factory for creating ChannelDecoder instances by string key.
 *
 * Registered keys: `TensorToPoint2D`, `TensorToMask2D`, `TensorToLine2D`,
 * `TensorToFeatureVector`.
 */
class DecoderFactory {
public:
    /**
     * @brief Create a decoder instance by name.
     *
     * @param decoder_name Factory key (e.g. `"TensorToMask2D"`).
     * @return Decoder instance, or `nullptr` if the name is not recognized.
     */
    [[nodiscard]] static std::unique_ptr<ChannelDecoder> create(std::string const & decoder_name);

    /**
     * @brief Get the list of all registered decoder names.
     *
     * @return Names accepted by @ref create (e.g. `"TensorToMask2D"`, `"TensorToPoint2D"`).
     */
    [[nodiscard]] static std::vector<std::string> availableDecoders();
};

}// namespace dl

#endif// NEURALYZER_DECODER_FACTORY_HPP
