#ifndef IENTITY_PROVIDER_H
#define IENTITY_PROVIDER_H

#include "TimeFrame/TimeFrame.hpp"
#include "CoreGeometry/lines.hpp"
#include "Entity/EntityTypes.hpp"

#include <cstddef>

/**
 * @brief Interface for sources that can expand a timestamp into multiple entities (e.g., lines).
 */
class IEntityProvider {
public:
    virtual ~IEntityProvider() = default;

    IEntityProvider() = default;
    IEntityProvider(IEntityProvider const&) = delete;
    auto operator=(IEntityProvider const&) -> IEntityProvider& = delete;
    IEntityProvider(IEntityProvider&&) = delete;
    auto operator=(IEntityProvider&&) -> IEntityProvider& = delete;

    /**
     * @brief Number of entities present at a given timestamp.
     */
    [[nodiscard]] virtual auto getEntityCountAt(TimeFrameIndex t) const -> size_t = 0;

    /**
     * @brief Optional convenience: get a specific line at timestamp and per-timestamp index.
     * Returns nullptr if out of range.
     */
    [[nodiscard]] virtual auto getLineAt(TimeFrameIndex t, int entityIndex) const -> Line2D const* = 0;

    /**
     * @brief Optional: get the EntityId for an entity at timestamp and local index, if available.
     * Implementors that don't support EntityIds may return 0.
     */
    [[nodiscard]] virtual auto getEntityIdAt(TimeFrameIndex t, int entityIndex) const -> EntityId { (void)t; (void)entityIndex; return 0; }
};

#endif // IENTITY_PROVIDER_H


