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
 * 
 * The EntityGroupManager can be null initially and set later via setGroupManager().
 * This allows getDefaultParameters() to return actual parameter objects while still
 * requiring the group manager to be set before execution.
 */
class GroupingTransformParametersBase : public TransformParametersBase {
public:
    explicit GroupingTransformParametersBase(EntityGroupManager* group_manager = nullptr)
        : group_manager_(group_manager) {}

    virtual ~GroupingTransformParametersBase() = default;

    /**
     * @brief Get the current EntityGroupManager
     * @return Pointer to the group manager, may be null
     */
    [[nodiscard]] EntityGroupManager* getGroupManager() const { return group_manager_; }

    /**
     * @brief Set the EntityGroupManager
     * @param group_manager Pointer to the group manager
     */
    void setGroupManager(EntityGroupManager* group_manager) { group_manager_ = group_manager; }

    /**
     * @brief Check if the group manager is valid
     * @return True if group manager is not null
     */
    [[nodiscard]] bool hasValidGroupManager() const { return group_manager_ != nullptr; }

private:
    EntityGroupManager* group_manager_;
};

#endif//WHISKERTOOLBOX_GROUPING_TRANSFORMS_HPP