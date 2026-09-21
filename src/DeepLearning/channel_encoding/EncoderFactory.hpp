#ifndef NEURALYZER_ENCODER_FACTORY_HPP
#define NEURALYZER_ENCODER_FACTORY_HPP

/**
 * @file EncoderFactory.hpp
 * @brief Factory for channel encoders that map DataManager geometry and images into
 *        model input tensors.
 *
 * @par Spatial scaling contract
 * Every encoder registered here must resize or reproject between
 * `source_image_size` (original media / mask canvas) and the tensor spatial
 * dimensions in `EncoderContext::height` / `EncoderContext::width` using the
 * **same convention** as the paired channel decoders in @ref DecoderFactory.
 *
 * - **Target convention:** pixel-center grid (`align_corners=false`), matching
 *   PyTorch `ImageEncoder` resize and OpenCV `cv2.resize` sampling geometry.
 * - **Continuous geometry** (points, lines): map pixel centers with
 *   `t = (x + 0.5) * T / I - 0.5` (and the inverse when decoding).
 * - **Discrete masks:** nearest-index raster ops on that grid (not independent
 *   per-point `round(x * scale)` formulas).
 *
 * Do not introduce ad-hoc scaling in individual encoders. Shared helpers will
 * live under `src/DeepLearning/spatial/SpatialResize.hpp` (see developer docs).
 *
 * @see DecoderFactory
 * @see EncoderDispatch
 */

#include "ChannelEncoder.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dl {

/**
 * @brief Factory for creating ChannelEncoder instances by string key.
 *
 * Registered keys: `ImageEncoder`, `Point2DEncoder`, `Mask2DEncoder`, `Line2DEncoder`.
 */
class EncoderFactory {
public:
    /**
     * @brief Create an encoder instance by name.
     *
     * @param encoder_name Factory key (e.g. `"ImageEncoder"`).
     * @return Encoder instance, or `nullptr` if the name is not recognized.
     */
    [[nodiscard]] static std::unique_ptr<ChannelEncoder> create(std::string const & encoder_name);

    /**
     * @brief Get the list of all registered encoder names.
     *
     * @return Names accepted by @ref create (e.g. `"ImageEncoder"`, `"Mask2DEncoder"`).
     */
    [[nodiscard]] static std::vector<std::string> availableEncoders();
};

}// namespace dl

#endif// NEURALYZER_ENCODER_FACTORY_HPP
