#ifndef TENSOR_DESIGN_SPEC_HPP
#define TENSOR_DESIGN_SPEC_HPP

/**
 * @file TensorDesignSpec.hpp
 * @brief Data structures describing a lazy-column tensor design specification.
 */

#include "TransformsV2/core/TensorColumnBuilders.hpp"

#include <string>
#include <vector>

namespace Neuralyzer::TensorDesign {

/**
 * @brief Row source type for a tensor design specification.
 */
enum class RowType : std::uint8_t {
    None,
    Interval,
    Timestamp,
    Ordinal,
    DerivedFromSource
};

/**
 * @brief Complete specification for building a lazy-column tensor.
 */
struct TensorDesignSpec {
    /// DataManager key under which the tensor will be stored (optional for build-only)
    std::string tensor_key;

    /// TimeKey used when registering the tensor in DataManager
    std::string output_time_key = "default";

    /// DataManager key for the row source data object
    std::string row_source_key;

    /// How rows are derived from the row source
    RowType row_type = RowType::None;

    /// Column recipes describing each lazy column
    std::vector<TensorBuilders::ColumnRecipe> columns;
};

}// namespace Neuralyzer::TensorDesign

#endif// TENSOR_DESIGN_SPEC_HPP
