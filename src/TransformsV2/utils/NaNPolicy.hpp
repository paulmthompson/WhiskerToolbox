/**
 * @file NaNPolicy.hpp
 * @brief Policy for handling NaN/Inf rows in dimensionality reduction transforms
 */

#ifndef NEURALYZER_V2_NANPOLICY_HPP
#define NEURALYZER_V2_NANPOLICY_HPP

namespace WhiskerToolbox::Transforms::V2 {

/**
 * @brief Policy for handling NaN/Inf rows in dimensionality reduction transforms
 */
enum class NaNPolicy {
    Fail,     ///< Return nullptr if any NaN/Inf rows are present
    Propagate,///< Skip NaN rows for computation, fill them with NaN in the output
    Drop      ///< Remove NaN rows entirely (output has fewer rows than input)
};

}// namespace WhiskerToolbox::Transforms::V2

#endif// NEURALYZER_V2_NANPOLICY_HPP
