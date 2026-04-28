/**
 * @file PostEncoderPipeline.cpp
 * @brief Implementation of the sequential post-encoder pipeline.
 */

#include "PostEncoderPipeline.hpp"

#include <torch/torch.h>

#include <cassert>
#include <stdexcept>

namespace dl {

void PostEncoderPipeline::add(std::unique_ptr<PostEncoderModule> module) {
    assert(module != nullptr && "PostEncoderPipeline::add: module must not be null");
    _modules.push_back(std::move(module));
}

std::string PostEncoderPipeline::name() const {
    return "pipeline";
}

at::Tensor PostEncoderPipeline::apply(at::Tensor const & features) const {
    assert(features.defined() && "PostEncoderPipeline: features must be defined");
    if (_modules.empty()) {
        return features;
    }
    at::Tensor current = features;
    for (auto const & module: _modules) {
        current = module->apply(current);
    }
    return current;
}

std::vector<int64_t>
PostEncoderPipeline::outputShape(std::vector<int64_t> const & input_shape) const {
    if (_modules.empty()) {
        return input_shape;
    }
    std::vector<int64_t> shape = input_shape;
    for (auto const & module: _modules) {
        shape = module->outputShape(shape);
    }
    return shape;
}

}// namespace dl
