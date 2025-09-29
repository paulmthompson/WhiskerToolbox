#ifndef WHISKERTOOLBOX_GROUPING_TRANSFORMS_HPP
#define WHISKERTOOLBOX_GROUPING_TRANSFORMS_HPP

#include "data_transforms.hpp"

#include <memory>

// Forward declaration
class EntityGroupManager;

/**
 * @brief Base class for transform parameters that need access to the EntityGroupManager
 * 
 * This extends the basic TransformParametersBase to provide access to the group management
 * system for operations that modify entity groups rather than creating new data.
 */
class GroupingTransformParametersBase : public TransformParametersBase {
public:
    explicit GroupingTransformParametersBase(EntityGroupManager* group_manager)
        : group_manager_(group_manager) {}

    virtual ~GroupingTransformParametersBase() = default;

    [[nodiscard]] EntityGroupManager* getGroupManager() const { return group_manager_; }

private:
    EntityGroupManager* group_manager_;
};

#endif//WHISKERTOOLBOX_GROUPING_TRANSFORMS_HPP