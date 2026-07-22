#ifndef NEURALYZER_CHANNEL_DECODER_HPP
#define NEURALYZER_CHANNEL_DECODER_HPP

/**
 * @file ChannelDecoder.hpp
 * @brief Base interface for channel decoders and per-decoder parameter structs.
 *
 * Channel decoders are the **tensor-to-DataManager** stage of the inference pipeline.
 * They read a selected slice of a model output `at::Tensor` and produce a concrete
 * geometry or tabular type that can be stored in the DataManager — for example
 * `Mask2D`, `Point2D<float>`, `Line2D`, or `std::vector<float>`.
 *
 * Typical pipeline position:
 * @code
 *   encoder output [B, C, H, W]
 *        → (optional) PostEncoderModule::apply()   // torch → torch
 *        → TensorToMask2D::decode() / …            // torch → DataManager type
 *        → DataManager::setData / addAtTime
 * @endcode
 *
 * Concrete decoders (`TensorToMask2D`, `TensorToPoint2D`, `TensorToLine2D`,
 * `TensorToFeatureVector`) implement static `decode()` methods and register via
 * `DecoderFactory`. `DecoderContext` carries indexing and coordinate-scaling state
 * populated by `SlotAssembler`; per-decoder params structs hold user settings.
 *
 * @see PostEncoderModule.hpp for optional tensor-to-tensor post-processing applied
 *      before decoding
 * @see DecoderFactory.hpp for decoder registration and lookup
 */

#include "CoreGeometry/ImageSize.hpp"

#include <string>

namespace dl {

/**
 * @brief Fields set by SlotAssembler from model metadata and runtime state.
 * 
 * Not user-configurable — describes how to index into the output tensor.
 */
struct DecoderContext {
    int source_channel = 0;       ///< Which channel to decode
    int batch_index = 0;          ///< Which batch index to read from
    int height = 256;             ///< Tensor spatial height
    int width = 256;              ///< Tensor spatial width
    ImageSize target_image_size{};///< Scale output back to original coords
};

/**
 * @brief User-configurable params for TensorToMask2D.
 */
struct MaskDecoderParams {
    float threshold = 0.5f;///< Binary threshold for mask generation
};

/**
 * @brief User-configurable params for TensorToPoint2D.
 */
struct PointDecoderParams {
    bool subpixel = true;  ///< Enable parabolic subpixel refinement
    float threshold = 0.5f;///< For decodeMultiple: minimum activation for local maxima
};

/**
 * @brief User-configurable params for TensorToLine2D.
 */
struct LineDecoderParams {
    float threshold = 0.5f;///< Binary threshold for line extraction
};

/**
 * @brief TensorToFeatureVector has no user-configurable params.
 */
struct FeatureVectorDecoderParams {};

/**
 * @brief Registry-facing interface for tensor-to-DataManager channel decoders.
 *
 * Subclasses identify themselves via `name()` and `outputTypeName()`. The actual
 * decode logic lives on static `decode()` methods of each concrete decoder class.
 *
 * @see PostEncoderModule for the complementary torch-to-torch processing stage
 */
class ChannelDecoder {
public:
    virtual ~ChannelDecoder() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::string outputTypeName() const = 0;
};

}// namespace dl

#endif// NEURALYZER_CHANNEL_DECODER_HPP
