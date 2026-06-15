/**
 * @file GeneralEncoderConfiguration.cpp
 * @brief Implementation of GeneralEncoderModel configuration helpers.
 */

#include "GeneralEncoderConfiguration.hpp"

#include <rfl/DefaultIfMissing.hpp>
#include <rfl/json.hpp>

#include "GeneralEncoderModel.hpp"

#include <sstream>

namespace dl {

namespace {

constexpr int kDefaultSpatialSize = 224;

}// namespace

std::vector<int64_t>
parseGeneralEncoderOutputShape(std::string const & shape_str) {
    std::vector<int64_t> out_shape;
    if (shape_str.empty()) {
        return out_shape;
    }

    std::istringstream iss(shape_str);
    std::string token;
    while (std::getline(iss, token, ',')) {
        try {
            auto const val = std::stoll(token);
            if (val > 0) {
                out_shape.push_back(val);
            }
        } catch (std::exception const &) {
            // Non-numeric token — skip it
        }
    }
    return out_shape;
}

GeneralEncoderModelParams
generalEncoderParamsForForm(GeneralEncoderModelParams const & stored) {
    GeneralEncoderModelParams form = stored;
    if (form.input_height <= 0) {
        form.input_height = kDefaultSpatialSize;
    }
    if (form.input_width <= 0) {
        form.input_width = kDefaultSpatialSize;
    }
    return form;
}

void applyGeneralEncoderConfiguration(
        GeneralEncoderModel & model,
        GeneralEncoderModelParams const & params) {
    int const height =
            params.input_height > 0 ? params.input_height : kDefaultSpatialSize;
    int const width =
            params.input_width > 0 ? params.input_width : kDefaultSpatialSize;
    model.setInputResolution(height, width);

    auto const out_shape = parseGeneralEncoderOutputShape(params.output_shape);
    if (!out_shape.empty()) {
        model.setOutputShape(out_shape);
    }
}

bool generalEncoderConfigurationComplete(GeneralEncoderModelParams const & params) {
    return params.shape_applied && params.input_height > 0 && params.input_width > 0;
}

std::string generalEncoderFormJsonFromStored(std::string const & stored_json) {
    auto parsed = rfl::json::read<GeneralEncoderModelParams, rfl::DefaultIfMissing>(
            stored_json);
    if (!parsed) {
        return stored_json;
    }
    return rfl::json::write(generalEncoderParamsForForm(*parsed));
}

std::string generalEncoderStoredJsonFromForm(
        std::string const & form_json,
        bool applied) {
    auto parsed = rfl::json::read<GeneralEncoderModelParams, rfl::DefaultIfMissing>(
            form_json);
    if (!parsed) {
        return form_json;
    }
    GeneralEncoderModelParams stored = *parsed;
    if (applied) {
        stored.shape_applied = true;
    }
    return rfl::json::write(stored);
}

}// namespace dl
