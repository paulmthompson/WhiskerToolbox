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

bool isSpatialDecoder(DecoderVariant const & params) {
    bool result = false;
    params.visit([&](auto const & decoder_params) {
        result = isSpatialDecoderParams<std::decay_t<decltype(decoder_params)>>();
    });
    return result;
}

template<typename DecoderParams>
std::string dataTypeForDecoderParams() {
    return dataTypeNameForParams<DecoderParams>();
}

template std::string dataTypeForDecoderParams<MaskDecoderParams>();
template std::string dataTypeForDecoderParams<PointDecoderParams>();
template std::string dataTypeForDecoderParams<LineDecoderParams>();
template std::string dataTypeForDecoderParams<FeatureVectorDecoderParams>();

std::string dataTypeForDecoder(DecoderVariant const & params) {
    std::string data_type;
    params.visit([&](auto const & decoder_params) {
        data_type = dataTypeForDecoderParams<std::decay_t<decltype(decoder_params)>>();
    });
    return data_type;
}

template<typename DecoderParams>
std::string decoderFactoryName() {
    return factoryNameForParams<DecoderParams>();
}

template std::string decoderFactoryName<MaskDecoderParams>();
template std::string decoderFactoryName<PointDecoderParams>();
template std::string decoderFactoryName<LineDecoderParams>();
template std::string decoderFactoryName<FeatureVectorDecoderParams>();

std::string decoderFactoryName(DecoderVariant const & params) {
    std::string factory_name;
    params.visit([&](auto const & decoder_params) {
        factory_name = decoderFactoryName<std::decay_t<decltype(decoder_params)>>();
    });
    return factory_name;
}

std::optional<DecoderVariant> decoderParamsFromFactoryName(
        std::string const & factory_name) {
    if (factory_name == "TensorToMask2D") {
        return DecoderVariant{MaskDecoderParams{}};
    }
    if (factory_name == "TensorToPoint2D") {
        return DecoderVariant{PointDecoderParams{}};
    }
    if (factory_name == "TensorToLine2D") {
        return DecoderVariant{LineDecoderParams{}};
    }
    if (factory_name == "TensorToFeatureVector") {
        return DecoderVariant{FeatureVectorDecoderParams{}};
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
        DecoderVariant const & params) {
    DecodedGeometryVariant result;
    params.visit([&](auto const & decoder_params) {
        result = decodeToGeometry(tensor, ctx, decoder_params);
    });
    return result;
}

}// namespace dl
