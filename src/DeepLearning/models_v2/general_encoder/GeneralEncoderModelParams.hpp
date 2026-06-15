/**
 * @file GeneralEncoderModelParams.hpp
 * @brief Serializable configuration parameters for GeneralEncoderModel.
 */

#ifndef WHISKERTOOLBOX_GENERAL_ENCODER_MODEL_PARAMS_HPP
#define WHISKERTOOLBOX_GENERAL_ENCODER_MODEL_PARAMS_HPP

#include <string>

namespace dl {

/**
 * @brief User-configurable parameters for @c GeneralEncoderModel.
 *
 * Persisted in workspace state under @c model_configurations["general_encoder"].
 */
struct GeneralEncoderModelParams {
    /// True after the user clicks Apply Shape in the UI.
    bool shape_applied = false;
    /// Input height in pixels; @c 0 means not yet configured.
    int input_height = 0;
    /// Input width in pixels; @c 0 means not yet configured.
    int input_width = 0;
    /// Comma-separated output dimensions (excluding batch), e.g. @c "768,16,16".
    std::string output_shape;
};

}// namespace dl

#endif// WHISKERTOOLBOX_GENERAL_ENCODER_MODEL_PARAMS_HPP
