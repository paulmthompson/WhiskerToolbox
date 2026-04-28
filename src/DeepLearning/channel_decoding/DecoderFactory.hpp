#ifndef WHISKERTOOLBOX_DECODER_FACTORY_HPP
#define WHISKERTOOLBOX_DECODER_FACTORY_HPP

#include "ChannelDecoder.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dl {

/**
 * @brief Factory for creating ChannelDecoder instances by string key.
 * 
 * The following keys are registered by default:
 *   - "TensorToPoint2D"         → TensorToPoint2D
 *   - "TensorToMask2D"          → TensorToMask2D
 *   - "TensorToLine2D"          → TensorToLine2D
 *   - "TensorToFeatureVector"   → TensorToFeatureVector
 */
class DecoderFactory {
public:
    /**
     * @brief Create a decoder instance by name.
     * 
     * Returns nullptr if the name is not recognized.
     */
    [[nodiscard]] static std::unique_ptr<ChannelDecoder> create(std::string const & decoder_name);

    /**
     * @brief Get the list of all registered decoder names.
     */
    [[nodiscard]] static std::vector<std::string> availableDecoders();
};

} // namespace dl

#endif // WHISKERTOOLBOX_DECODER_FACTORY_HPP
