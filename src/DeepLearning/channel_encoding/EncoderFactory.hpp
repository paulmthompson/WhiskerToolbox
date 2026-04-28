#ifndef WHISKERTOOLBOX_ENCODER_FACTORY_HPP
#define WHISKERTOOLBOX_ENCODER_FACTORY_HPP

#include "ChannelEncoder.hpp"

#include <memory>   // std::unique_ptr
#include <string>   // std::string
#include <vector>   // std::vector

namespace dl {

/// Factory for creating ChannelEncoder instances by string key.
///
/// The following keys are registered by default:
///   - "ImageEncoder"     → ImageEncoder
///   - "Point2DEncoder"   → Point2DEncoder
///   - "Mask2DEncoder"    → Mask2DEncoder
///   - "Line2DEncoder"    → Line2DEncoder
class EncoderFactory {
public:
    /**
     * @brief Create an encoder instance by name.
     * 
     * Returns nullptr if the name is not recognized.
     */
    /// Returns nullptr if the name is not recognized.
    [[nodiscard]] static std::unique_ptr<ChannelEncoder> create(std::string const & encoder_name);

    /**
     * @brief Get the list of all registered encoder names.
     */
    [[nodiscard]] static std::vector<std::string> availableEncoders();
};

} // namespace dl

#endif // WHISKERTOOLBOX_ENCODER_FACTORY_HPP
