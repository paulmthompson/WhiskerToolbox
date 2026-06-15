/// @file EncoderDecoderBindingTypes.hpp
/// @brief rfl::TaggedUnion encoder/decoder parameter types for slot bindings.

#ifndef DEEP_LEARNING_ENCODER_DECODER_BINDING_TYPES_HPP
#define DEEP_LEARNING_ENCODER_DECODER_BINDING_TYPES_HPP

#include "channel_decoding/ChannelDecoder.hpp"
#include "channel_encoding/ChannelEncoder.hpp"

#include <rfl.hpp>

namespace dl {

/**
 * @brief Tagged union discriminated by "encoder".
 *
 * User-configurable encoder parameters for all registered channel encoders.
 */
using EncoderVariant = rfl::TaggedUnion<
        "encoder",
        ImageEncoderParams,
        Point2DEncoderParams,
        Mask2DEncoderParams,
        Line2DEncoderParams>;

/**
 * @brief Tagged union discriminated by "decoder".
 *
 * User-configurable decoder parameters for all registered channel decoders.
 */
using DecoderVariant = rfl::TaggedUnion<
        "decoder",
        MaskDecoderParams,
        PointDecoderParams,
        LineDecoderParams,
        FeatureVectorDecoderParams>;

}// namespace dl

#endif// DEEP_LEARNING_ENCODER_DECODER_BINDING_TYPES_HPP
