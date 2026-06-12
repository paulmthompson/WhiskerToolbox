/**
 * @file DecoderDispatch.cpp
 * @brief Implementation of type-safe channel decoder dispatch.
 */

#include "DecoderDispatch.hpp"

#include "TensorToFeatureVector.hpp"
#include "TensorToLine2D.hpp"
#include "TensorToMask2D.hpp"
#include "TensorToPoint2D.hpp"

#include <ATen/core/Tensor.h>

#include <type_traits>

namespace dl {

namespace {

template<typename DecoderParams>
[[nodiscard]] constexpr char const * factoryNameForParams() {
    if constexpr (std::is_same_v<DecoderParams, MaskDecoderParams>) {
        return "TensorToMask2D";
    } else if constexpr (std::is_same_v<DecoderParams, PointDecoderParams>) {
        return "TensorToPoint2D";
    } else if constexpr (std::is_same_v<DecoderParams, LineDecoderParams>) {
        return "TensorToLine2D";
    } else if constexpr (std::is_same_v<DecoderParams, FeatureVectorDecoderParams>) {
        return "TensorToFeatureVector";
    } else {
        return "";
    }
}

template<typename DecoderParams>
[[nodiscard]] constexpr char const * dataTypeNameForParams() {
    if constexpr (std::is_same_v<DecoderParams, MaskDecoderParams>) {
        return "MaskData";
    } else if constexpr (std::is_same_v<DecoderParams, PointDecoderParams>) {
        return "PointData";
    } else if constexpr (std::is_same_v<DecoderParams, LineDecoderParams>) {
        return "LineData";
    } else if constexpr (std::is_same_v<DecoderParams, FeatureVectorDecoderParams>) {
        return "TensorData";
    } else {
        return "";
    }
}

}// namespace

bool isSpatialDecoder(DecoderParamsVariant const & params) {
    return std::visit(
            []<typename DecoderParams>(DecoderParams const &) {
                return isSpatialDecoderParams<DecoderParams>();
            },
            params);
}

template<typename DecoderParams>
std::string dataTypeForDecoderParams() {
    return dataTypeNameForParams<DecoderParams>();
}

template std::string dataTypeForDecoderParams<MaskDecoderParams>();
template std::string dataTypeForDecoderParams<PointDecoderParams>();
template std::string dataTypeForDecoderParams<LineDecoderParams>();
template std::string dataTypeForDecoderParams<FeatureVectorDecoderParams>();

std::string dataTypeForDecoder(DecoderParamsVariant const & params) {
    return std::visit(
            []<typename DecoderParams>(DecoderParams const &) {
                return dataTypeForDecoderParams<DecoderParams>();
            },
            params);
}

template<typename DecoderParams>
std::string decoderFactoryName() {
    return factoryNameForParams<DecoderParams>();
}

template std::string decoderFactoryName<MaskDecoderParams>();
template std::string decoderFactoryName<PointDecoderParams>();
template std::string decoderFactoryName<LineDecoderParams>();
template std::string decoderFactoryName<FeatureVectorDecoderParams>();

std::string decoderFactoryName(DecoderParamsVariant const & params) {
    return std::visit(
            []<typename DecoderParams>(DecoderParams const &) {
                return decoderFactoryName<DecoderParams>();
            },
            params);
}

std::optional<DecoderParamsVariant> decoderParamsFromFactoryName(
        std::string const & factory_name) {
    if (factory_name == "TensorToMask2D") {
        return MaskDecoderParams{};
    }
    if (factory_name == "TensorToPoint2D") {
        return PointDecoderParams{};
    }
    if (factory_name == "TensorToLine2D") {
        return LineDecoderParams{};
    }
    if (factory_name == "TensorToFeatureVector") {
        return FeatureVectorDecoderParams{};
    }
    return std::nullopt;
}

template<typename DecoderParams>
DecodedGeometryVariant decodeToGeometry(
        at::Tensor const & tensor,
        DecoderContext const & ctx,
        DecoderParams const & params) {
    if constexpr (std::is_same_v<DecoderParams, MaskDecoderParams>) {
        return TensorToMask2D::decode(tensor, ctx, params);
    } else if constexpr (std::is_same_v<DecoderParams, PointDecoderParams>) {
        return TensorToPoint2D::decode(tensor, ctx, params);
    } else if constexpr (std::is_same_v<DecoderParams, LineDecoderParams>) {
        return TensorToLine2D::decode(tensor, ctx, params);
    } else if constexpr (std::is_same_v<DecoderParams, FeatureVectorDecoderParams>) {
        return TensorToFeatureVector::decode(tensor, ctx, params);
    } else {
        static_assert(sizeof(DecoderParams) == 0, "Unsupported decoder params type");
    }
}

template DecodedGeometryVariant decodeToGeometry<MaskDecoderParams>(
        at::Tensor const &, DecoderContext const &, MaskDecoderParams const &);
template DecodedGeometryVariant decodeToGeometry<PointDecoderParams>(
        at::Tensor const &, DecoderContext const &, PointDecoderParams const &);
template DecodedGeometryVariant decodeToGeometry<LineDecoderParams>(
        at::Tensor const &, DecoderContext const &, LineDecoderParams const &);
template DecodedGeometryVariant decodeToGeometry<FeatureVectorDecoderParams>(
        at::Tensor const &, DecoderContext const &, FeatureVectorDecoderParams const &);

DecodedGeometryVariant decodeToGeometry(
        at::Tensor const & tensor,
        DecoderContext const & ctx,
        DecoderParamsVariant const & params) {
    return std::visit(
            [&](auto const & decoder_params) {
                return decodeToGeometry(tensor, ctx, decoder_params);
            },
            params);
}

}// namespace dl
