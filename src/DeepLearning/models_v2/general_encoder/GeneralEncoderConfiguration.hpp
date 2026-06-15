/**
 * @file GeneralEncoderConfiguration.hpp
 * @brief Apply and validate GeneralEncoderModelParams against GeneralEncoderModel.
 */

#ifndef WHISKERTOOLBOX_GENERAL_ENCODER_CONFIGURATION_HPP
#define WHISKERTOOLBOX_GENERAL_ENCODER_CONFIGURATION_HPP

#include "GeneralEncoderModelParams.hpp"

#include <cstdint>
#include <vector>

namespace dl {

class GeneralEncoderModel;

/**
 * @brief Parse a comma-separated output shape string into dimensions.
 *
 * @return Parsed positive dimensions; empty if @p shape_str is empty or invalid.
 */
[[nodiscard]] std::vector<int64_t>
parseGeneralEncoderOutputShape(std::string const & shape_str);

/**
 * @brief Build form-friendly params for AutoParamWidget display.
 *
 * Unconfigured height/width (@c 0) are shown as @c 224.
 */
[[nodiscard]] GeneralEncoderModelParams
generalEncoderParamsForForm(GeneralEncoderModelParams const & stored);

/**
 * @brief Apply persisted configuration to a live model instance.
 *
 * @pre @p model is a GeneralEncoderModel instance.
 */
void applyGeneralEncoderConfiguration(
        GeneralEncoderModel & model,
        GeneralEncoderModelParams const & params);

/**
 * @brief Whether configuration is complete enough to load weights.
 */
[[nodiscard]] bool
generalEncoderConfigurationComplete(GeneralEncoderModelParams const & params);

/**
 * @brief Convert stored configuration JSON to form JSON for AutoParamWidget.
 */
[[nodiscard]] std::string
generalEncoderFormJsonFromStored(std::string const & stored_json);

/**
 * @brief Convert form JSON to stored configuration JSON.
 */
[[nodiscard]] std::string
generalEncoderStoredJsonFromForm(std::string const & form_json, bool applied);

}// namespace dl

#endif// WHISKERTOOLBOX_GENERAL_ENCODER_CONFIGURATION_HPP
