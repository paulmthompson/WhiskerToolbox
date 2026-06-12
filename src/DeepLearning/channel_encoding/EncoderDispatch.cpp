/**
 * @file EncoderDispatch.cpp
 * @brief Implementation of type-safe channel encoder dispatch.
 */

#include "EncoderDispatch.hpp"

#include "ImageEncoder.hpp"
#include "Line2DEncoder.hpp"
#include "Mask2DEncoder.hpp"
#include "Point2DEncoder.hpp"

#include <ATen/core/Tensor.h>

#include <iostream>
#include <type_traits>

namespace dl {

namespace {

template<typename EncoderParams>
[[nodiscard]] constexpr char const * factoryNameForParams() {
    if constexpr (std::is_same_v<EncoderParams, ImageEncoderParams>) {
        return "ImageEncoder";
    } else if constexpr (std::is_same_v<EncoderParams, Point2DEncoderParams>) {
        return "Point2DEncoder";
    } else if constexpr (std::is_same_v<EncoderParams, Mask2DEncoderParams>) {
        return "Mask2DEncoder";
    } else if constexpr (std::is_same_v<EncoderParams, Line2DEncoderParams>) {
        return "Line2DEncoder";
    } else {
        return "";
    }
}

template<typename EncoderParams>
[[nodiscard]] constexpr char const * dataTypeNameForParams() {
    if constexpr (std::is_same_v<EncoderParams, ImageEncoderParams>) {
        return "MediaData";
    } else if constexpr (std::is_same_v<EncoderParams, Point2DEncoderParams>) {
        return "PointData";
    } else if constexpr (std::is_same_v<EncoderParams, Mask2DEncoderParams>) {
        return "MaskData";
    } else if constexpr (std::is_same_v<EncoderParams, Line2DEncoderParams>) {
        return "LineData";
    } else {
        return "";
    }
}

[[nodiscard]] ImageSize const & sourceSizeForEncoding(
        ImageSize const & source_image_size,
        EncoderContext const & ctx) {
    if (source_image_size.width > 0 && source_image_size.height > 0) {
        return source_image_size;
    }
    static thread_local ImageSize fallback;
    fallback = ImageSize{ctx.width, ctx.height};
    return fallback;
}

}// namespace

bool isImageEncoder(EncoderParamsVariant const & params) {
    return std::visit(
            []<typename EncoderParams>(EncoderParams const &) {
                return isImageEncoderParams<EncoderParams>();
            },
            params);
}

template<typename EncoderParams>
std::string dataTypeForEncoderParams() {
    return dataTypeNameForParams<EncoderParams>();
}

template std::string dataTypeForEncoderParams<ImageEncoderParams>();
template std::string dataTypeForEncoderParams<Point2DEncoderParams>();
template std::string dataTypeForEncoderParams<Mask2DEncoderParams>();
template std::string dataTypeForEncoderParams<Line2DEncoderParams>();

std::string dataTypeForEncoder(EncoderParamsVariant const & params) {
    return std::visit(
            []<typename EncoderParams>(EncoderParams const &) {
                return dataTypeForEncoderParams<EncoderParams>();
            },
            params);
}

template<typename EncoderParams>
std::string encoderFactoryName() {
    return factoryNameForParams<EncoderParams>();
}

template std::string encoderFactoryName<ImageEncoderParams>();
template std::string encoderFactoryName<Point2DEncoderParams>();
template std::string encoderFactoryName<Mask2DEncoderParams>();
template std::string encoderFactoryName<Line2DEncoderParams>();

std::string encoderFactoryName(EncoderParamsVariant const & params) {
    return std::visit(
            []<typename EncoderParams>(EncoderParams const &) {
                return encoderFactoryName<EncoderParams>();
            },
            params);
}

std::optional<EncoderParamsVariant> encoderParamsFromFactoryName(
        std::string const & factory_name) {
    if (factory_name == "ImageEncoder") {
        return ImageEncoderParams{};
    }
    if (factory_name == "Point2DEncoder") {
        return Point2DEncoderParams{};
    }
    if (factory_name == "Mask2DEncoder") {
        return Mask2DEncoderParams{};
    }
    if (factory_name == "Line2DEncoder") {
        return Line2DEncoderParams{};
    }
    return std::nullopt;
}

template<typename EncoderParams>
void encodeToTensor(
        EncodingSourceVariant const & source,
        at::Tensor & tensor,
        EncoderContext const & ctx,
        ImageSize source_image_size,
        EncoderParams const & params) {
    ImageSize const & actual_source = sourceSizeForEncoding(source_image_size, ctx);
    if constexpr (std::is_same_v<EncoderParams, ImageEncoderParams>) {
        auto const * image = std::get_if<ImageEncodingSource>(&source);
        if (!image) {
            std::cerr << "EncoderDispatch: expected ImageEncodingSource for ImageEncoder\n";
            return;
        }
        if (image->is_8bit) {
            ImageEncoder::encode(
                    image->data_u8,
                    image->size,
                    image->channels,
                    tensor,
                    ctx,
                    params);
        } else {
            ImageEncoder::encode(
                    image->data_f32,
                    image->size,
                    image->channels,
                    tensor,
                    ctx,
                    params);
        }
    } else if constexpr (std::is_same_v<EncoderParams, Point2DEncoderParams>) {
        auto const * points = std::get_if<std::vector<Point2D<float>>>(&source);
        if (!points) {
            std::cerr << "EncoderDispatch: expected point vector for Point2DEncoder\n";
            return;
        }
        Point2DEncoder::encode(*points, actual_source, tensor, ctx, params);
    } else if constexpr (std::is_same_v<EncoderParams, Mask2DEncoderParams>) {
        auto const * mask = std::get_if<Mask2D>(&source);
        if (!mask) {
            std::cerr << "EncoderDispatch: expected Mask2D for Mask2DEncoder\n";
            return;
        }
        Mask2DEncoder::encode(*mask, actual_source, tensor, ctx, params);
    } else if constexpr (std::is_same_v<EncoderParams, Line2DEncoderParams>) {
        auto const * line = std::get_if<Line2D>(&source);
        if (!line) {
            std::cerr << "EncoderDispatch: expected Line2D for Line2DEncoder\n";
            return;
        }
        Line2DEncoder::encode(*line, actual_source, tensor, ctx, params);
    } else {
        static_assert(sizeof(EncoderParams) == 0, "Unsupported encoder params type");
    }
}

template void encodeToTensor<ImageEncoderParams>(
        EncodingSourceVariant const &,
        at::Tensor &,
        EncoderContext const &,
        ImageSize,
        ImageEncoderParams const &);
template void encodeToTensor<Point2DEncoderParams>(
        EncodingSourceVariant const &,
        at::Tensor &,
        EncoderContext const &,
        ImageSize,
        Point2DEncoderParams const &);
template void encodeToTensor<Mask2DEncoderParams>(
        EncodingSourceVariant const &,
        at::Tensor &,
        EncoderContext const &,
        ImageSize,
        Mask2DEncoderParams const &);
template void encodeToTensor<Line2DEncoderParams>(
        EncodingSourceVariant const &,
        at::Tensor &,
        EncoderContext const &,
        ImageSize,
        Line2DEncoderParams const &);

void encodeToTensor(
        EncodingSourceVariant const & source,
        at::Tensor & tensor,
        EncoderContext const & ctx,
        ImageSize source_image_size,
        EncoderParamsVariant const & params) {
    std::visit(
            [&](auto const & encoder_params) {
                encodeToTensor(source, tensor, ctx, source_image_size, encoder_params);
            },
            params);
}

}// namespace dl
