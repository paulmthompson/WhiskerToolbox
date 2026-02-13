#include "EncoderFactory.hpp"

#include "ImageEncoder.hpp"
#include "Line2DEncoder.hpp"
#include "Mask2DEncoder.hpp"
#include "Point2DEncoder.hpp"

namespace dl {

std::unique_ptr<ChannelEncoder> EncoderFactory::create(std::string const & encoder_name)
{
    if (encoder_name == "ImageEncoder") {
        return std::make_unique<ImageEncoder>();
    }
    if (encoder_name == "Point2DEncoder") {
        return std::make_unique<Point2DEncoder>();
    }
    if (encoder_name == "Mask2DEncoder") {
        return std::make_unique<Mask2DEncoder>();
    }
    if (encoder_name == "Line2DEncoder") {
        return std::make_unique<Line2DEncoder>();
    }
    return nullptr;
}

std::vector<std::string> EncoderFactory::availableEncoders()
{
    return {"ImageEncoder", "Point2DEncoder", "Mask2DEncoder", "Line2DEncoder"};
}

} // namespace dl
