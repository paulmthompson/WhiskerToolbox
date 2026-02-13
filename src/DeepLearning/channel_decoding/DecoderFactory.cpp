#include "DecoderFactory.hpp"

#include "TensorToLine2D.hpp"
#include "TensorToMask2D.hpp"
#include "TensorToPoint2D.hpp"

namespace dl {

std::unique_ptr<ChannelDecoder> DecoderFactory::create(std::string const & decoder_name)
{
    if (decoder_name == "TensorToPoint2D") {
        return std::make_unique<TensorToPoint2D>();
    }
    if (decoder_name == "TensorToMask2D") {
        return std::make_unique<TensorToMask2D>();
    }
    if (decoder_name == "TensorToLine2D") {
        return std::make_unique<TensorToLine2D>();
    }
    return nullptr;
}

std::vector<std::string> DecoderFactory::availableDecoders()
{
    return {"TensorToPoint2D", "TensorToMask2D", "TensorToLine2D"};
}

} // namespace dl
