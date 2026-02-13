#include "ImageEncoder.hpp"

#include <stdexcept>

namespace dl {

std::string ImageEncoder::name() const
{
    return "ImageEncoder";
}

std::string ImageEncoder::inputTypeName() const
{
    return "Image";
}

void ImageEncoder::encode(std::vector<uint8_t> const & image_data,
                          ImageSize const source_size,
                          int const num_channels,
                          torch::Tensor & tensor,
                          EncoderParams const & params) const
{
    if (num_channels != 1 && num_channels != 3) {
        throw std::invalid_argument("ImageEncoder: num_channels must be 1 or 3, got " +
                                    std::to_string(num_channels));
    }

    auto const expected_size =
        static_cast<size_t>(source_size.height) * source_size.width * num_channels;
    if (image_data.size() != expected_size) {
        throw std::invalid_argument(
            "ImageEncoder: image_data size mismatch. Expected " +
            std::to_string(expected_size) + ", got " +
            std::to_string(image_data.size()));
    }

    // Create a temporary tensor from the source image: [H, W, C]
    auto src_tensor = torch::from_blob(
        const_cast<uint8_t *>(image_data.data()),
        {source_size.height, source_size.width, num_channels},
        torch::kByte);
    
    // Check if output tensor is uint8 — if so, skip float conversion
    bool const output_is_uint8 = (tensor.dtype() == torch::kByte);
    
    if (output_is_uint8) {
        // Keep as uint8, no normalization
        // Permute to [C, H, W]
        src_tensor = src_tensor.permute({2, 0, 1}).clone();
        
        // Resize if needed (need float for interpolation, then back to uint8)
        if (source_size.height != params.height || source_size.width != params.width) {
            src_tensor = src_tensor.to(torch::kFloat32).unsqueeze(0);
            src_tensor = torch::nn::functional::interpolate(
                src_tensor,
                torch::nn::functional::InterpolateFuncOptions()
                    .size(std::vector<int64_t>{params.height, params.width})
                    .mode(torch::kBilinear)
                    .align_corners(false));
            src_tensor = src_tensor.squeeze(0).clamp(0, 255).to(torch::kByte);
        }
    } else {
        // Convert to float for float32 output
        src_tensor = src_tensor.to(torch::kFloat32);

        // Normalize uint8 [0,255] -> [0,1]
        if (params.normalize) {
            src_tensor = src_tensor / 255.0f;
        }

        // Permute to [C, H, W]
        src_tensor = src_tensor.permute({2, 0, 1});

        // Resize to target spatial dimensions if needed
        if (source_size.height != params.height || source_size.width != params.width) {
            src_tensor = src_tensor.unsqueeze(0);
            src_tensor = torch::nn::functional::interpolate(
                src_tensor,
                torch::nn::functional::InterpolateFuncOptions()
                    .size(std::vector<int64_t>{params.height, params.width})
                    .mode(torch::kBilinear)
                    .align_corners(false));
            src_tensor = src_tensor.squeeze(0);
        }
    }

    // Determine how many output channels to write
    auto const tensor_channels = tensor.size(1);
    auto const available_output = tensor_channels - params.target_channel;

    if (num_channels == 1 && available_output >= 3) {
        // Replicate grayscale to 3 channels
        for (int c = 0; c < 3; ++c) {
            tensor[params.batch_index][params.target_channel + c] = src_tensor[0];
        }
    } else {
        // Copy source channels directly
        auto const channels_to_write = std::min(static_cast<int64_t>(num_channels), available_output);
        for (int64_t c = 0; c < channels_to_write; ++c) {
            tensor[params.batch_index][params.target_channel + c] = src_tensor[c];
        }
    }
}

void ImageEncoder::encode(std::vector<float> const & image_data,
                          ImageSize const source_size,
                          int const num_channels,
                          torch::Tensor & tensor,
                          EncoderParams const & params) const
{
    if (num_channels != 1 && num_channels != 3) {
        throw std::invalid_argument("ImageEncoder: num_channels must be 1 or 3, got " +
                                    std::to_string(num_channels));
    }

    auto const expected_size =
        static_cast<size_t>(source_size.height) * source_size.width * num_channels;
    if (image_data.size() != expected_size) {
        throw std::invalid_argument(
            "ImageEncoder: image_data size mismatch. Expected " +
            std::to_string(expected_size) + ", got " +
            std::to_string(image_data.size()));
    }

    // Create a temporary tensor from the source image: [H, W, C]
    auto src_tensor = torch::from_blob(
        const_cast<float *>(image_data.data()),
        {source_size.height, source_size.width, num_channels},
        torch::kFloat32);

    // Clone to own the data
    src_tensor = src_tensor.clone();

    // Normalize if requested (float data may already be in [0,1])
    if (params.normalize) {
        auto const max_val = src_tensor.max().item<float>();
        if (max_val > 1.0f) {
            src_tensor = src_tensor / max_val;
        }
    }

    // Permute to [C, H, W]
    src_tensor = src_tensor.permute({2, 0, 1});

    // Resize to target spatial dimensions if needed
    if (source_size.height != params.height || source_size.width != params.width) {
        src_tensor = src_tensor.unsqueeze(0);
        src_tensor = torch::nn::functional::interpolate(
            src_tensor,
            torch::nn::functional::InterpolateFuncOptions()
                .size(std::vector<int64_t>{params.height, params.width})
                .mode(torch::kBilinear)
                .align_corners(false));
        src_tensor = src_tensor.squeeze(0);
    }

    // Determine how many output channels to write
    auto const tensor_channels = tensor.size(1);
    auto const available_output = tensor_channels - params.target_channel;

    if (num_channels == 1 && available_output >= 3) {
        // Replicate grayscale to 3 channels
        for (int c = 0; c < 3; ++c) {
            tensor[params.batch_index][params.target_channel + c] = src_tensor[0];
        }
    } else {
        auto const channels_to_write = std::min(static_cast<int64_t>(num_channels), available_output);
        for (int64_t c = 0; c < channels_to_write; ++c) {
            tensor[params.batch_index][params.target_channel + c] = src_tensor[c];
        }
    }
}

} // namespace dl
