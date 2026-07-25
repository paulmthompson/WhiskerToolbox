#ifndef TENSOR_DESIGN_BUILDER_HPP
#define TENSOR_DESIGN_BUILDER_HPP

/**
 * @file TensorDesignBuilder.hpp
 * @brief Qt-free builder for lazy-column tensors from JSON design specifications.
 */

#include "TensorDesignSpec.hpp"

#include <optional>
#include <string>

class DataManager;
class TensorData;

namespace Neuralyzer::TensorDesign {

/**
 * @brief Parse a TensorDesigner-compatible JSON envelope into a design spec.
 * @param json JSON string with row_source and columns fields
 * @return Parsed spec, or nullopt if parsing fails
 */
[[nodiscard]] std::optional<TensorDesignSpec> parseDesignJson(std::string const & json);

/**
 * @brief Serialize a design spec to a TensorDesigner-compatible JSON envelope.
 * @param spec Design specification to serialize
 * @return JSON string (pretty-printed with 2-space indent)
 */
[[nodiscard]] std::string serializeDesignJson(TensorDesignSpec const & spec);

/**
 * @brief Build a lazy TensorData from a design spec without registering it.
 * @pre spec.row_type must not be None or Ordinal
 * @pre spec.columns must not be empty
 * @param dm DataManager containing row source and column source data
 * @param spec Design specification
 * @return Built tensor, or nullopt on failure
 */
[[nodiscard]] std::optional<TensorData> buildTensor(
        DataManager & dm,
        TensorDesignSpec const & spec);

/**
 * @brief Parse JSON and build a lazy TensorData.
 * @param dm DataManager containing source data
 * @param json TensorDesigner-compatible JSON envelope
 * @return Built tensor, or nullopt on failure
 */
[[nodiscard]] std::optional<TensorData> buildTensorFromDesignJson(
        DataManager & dm,
        std::string const & json);

/**
 * @brief Build a tensor from a design spec and register it in DataManager.
 * @pre spec.tensor_key must not be empty
 * @param dm DataManager to populate
 * @param spec Design specification (tensor_key required)
 * @return true if the tensor was built and registered
 */
bool populateDataManager(DataManager & dm, TensorDesignSpec const & spec);

}// namespace Neuralyzer::TensorDesign

#endif// TENSOR_DESIGN_BUILDER_HPP
