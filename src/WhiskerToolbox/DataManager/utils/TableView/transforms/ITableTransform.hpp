#ifndef ITABLETRANSFORM_HPP
#define ITABLETRANSFORM_HPP

#include "utils/TableView/core/TableView.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Interface for table-to-table transformations.
 */
class ITableTransform {
public:
    virtual ~ITableTransform() = default;

    /**
     * @brief Name of the transform.
     */
    [[nodiscard]] virtual auto name() const -> std::string = 0;

    /**
     * @brief Apply the transform to a source table to produce a new table.
     *
     * @param source Source `TableView`.
     * @return A new `TableView` representing the transformed data.
     */
    [[nodiscard]] virtual auto apply(TableView const & source) -> TableView = 0;
};

#endif // ITABLETRANSFORM_HPP


