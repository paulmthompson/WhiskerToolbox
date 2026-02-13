#ifndef WHISKERTOOLBOX_LINE2D_ENCODER_HPP
#define WHISKERTOOLBOX_LINE2D_ENCODER_HPP

#include "ChannelEncoder.hpp"

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"

namespace dl {

/// Encodes a Line2D (ordered polyline) into a tensor channel.
///
/// Supported modes:
///   - Binary:  Rasterizes the polyline using Bresenham-style line drawing
///   - Heatmap: Rasterizes the polyline with Gaussian blur along the line
///
/// Line point coordinates are scaled from source ImageSize to (H, W).
class Line2DEncoder : public ChannelEncoder {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string inputTypeName() const override;

    /// Encode a polyline into tensor[batch_index, target_channel, :, :]
    void encode(Line2D const & line,
                ImageSize source_size,
                torch::Tensor & tensor,
                EncoderParams const & params) const;
};

} // namespace dl

#endif // WHISKERTOOLBOX_LINE2D_ENCODER_HPP
