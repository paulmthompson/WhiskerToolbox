#ifndef WHISKERTOOLBOX_ENTITY_RESOLVER_HPP
#define WHISKERTOOLBOX_ENTITY_RESOLVER_HPP

#include "Entity/EntityTypes.hpp"
#include "Entity/Lineage/LineageTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

// Forward declarations
class DataManager;

namespace WhiskerToolbox::Lineage {
class DataManagerEntityDataSource;
}

namespace WhiskerToolbox::Entity::Lineage {

// Forward declaration
class LineageResolver;

/**
 * @brief Resolves derived data elements back to their source EntityIds
 * 
 * The EntityResolver uses lineage metadata stored in the LineageRegistry
 * to trace derived values back to the source entities that produced them.
 * 
 * This enables:
 * - Clicking on a derived value and finding the source entity
 * - Filtering derived data and creating groups from source entities
 * - Understanding data provenance through transformation chains
 * 
 * @example
 * @code
 * // Given: MaskData "masks" → AnalogTimeSeries "mask_areas" (via MaskArea transform)
 * EntityResolver resolver(dm);
 * 
 * // Find which mask produced the area value at time T5
 * auto source_ids = resolver.resolveToSource("mask_areas", TimeFrameIndex(5));
 * // source_ids contains the EntityId(s) of masks at time 5
 * @endcode
 */
class EntityResolver {
public:
    /**
     * @brief Construct an EntityResolver
     * 
     * @param dm Pointer to the DataManager (must outlive this resolver)
     */
    explicit EntityResolver(DataManager * dm);

    /**
     * @brief Destructor (defined in .cpp to allow forward-declared unique_ptr members)
     */
    ~EntityResolver();

    // =========================================================================
    // Time-based Resolution
    // =========================================================================

    /**
     * @brief Resolve derived element to source EntityIds (single step)
     * 
     * Looks up the lineage for the given data key and returns the EntityIds
     * from the immediate source container that correspond to the given time
     * and local index.
     * 
     * @param data_key Key of the (possibly derived) container
     * @param time TimeFrameIndex of element
     * @param local_index For ragged containers, which element at that time (0-indexed)
     * @return Vector of source EntityIds (empty if no lineage or not found)
     * 
     * @note For Source lineage, returns EntityIds from the container itself
     * @note For OneToOneByTime, returns EntityIds from source at same time
     * @note For AllToOneByTime, returns ALL EntityIds from source at that time
     */
    [[nodiscard]] std::vector<EntityId> resolveToSource(
            std::string const & data_key,
            TimeFrameIndex time,
            std::size_t local_index = 0) const;

    /**
     * @brief Resolve all the way to root source containers
     * 
     * Traverses the lineage chain until reaching containers with Source lineage.
     * This handles multi-level derivations like:
     *   MaskData → AnalogTimeSeries (areas) → DigitalEventSeries (peaks)
     * 
     * @param data_key Key of the derived container
     * @param time TimeFrameIndex of element
     * @param local_index For ragged containers, which element at that time
     * @return Vector of root source EntityIds
     */
    [[nodiscard]] std::vector<EntityId> resolveToRoot(
            std::string const & data_key,
            TimeFrameIndex time,
            std::size_t local_index = 0) const;

    // =========================================================================
    // EntityId-based Resolution (for entity-bearing derived containers)
    // =========================================================================

    /**
     * @brief Resolve by this container's EntityId to parent EntityIds
     * 
     * For containers that have their own EntityIds (like LineData from MaskData),
     * this maps from the derived entity's ID to its parent entity's ID(s).
     * 
     * @param data_key Key of the derived container
     * @param derived_entity_id EntityId of element in this container
     * @return Vector of parent EntityIds (empty if no mapping found)
     * 
     * @note Requires EntityMappedLineage or ImplicitEntityMapping lineage type
     */
    [[nodiscard]] std::vector<EntityId> resolveByEntityId(
            std::string const & data_key,
            EntityId derived_entity_id) const;

    /**
     * @brief Resolve by EntityId all the way to root
     * 
     * For entity-bearing derived containers, traces the full lineage chain
     * from derived EntityId to root source EntityIds.
     * 
     * @param data_key Key of the derived container
     * @param derived_entity_id EntityId of element in this container
     * @return Vector of root source EntityIds
     */
    [[nodiscard]] std::vector<EntityId> resolveByEntityIdToRoot(
            std::string const & data_key,
            EntityId derived_entity_id) const;

    // =========================================================================
    // Bulk Resolution / Queries
    // =========================================================================

    /**
     * @brief Get all source EntityIds for a derived container
     * 
     * Returns all EntityIds from the source container(s) that contributed
     * to any element in the derived container.
     * 
     * @param data_key Key of the derived container
     * @return Set of all contributing source EntityIds
     */
    [[nodiscard]] std::unordered_set<EntityId> getAllSourceEntities(
            std::string const & data_key) const;

    /**
     * @brief Get lineage chain for a data key
     * 
     * Returns the sequence of data keys from the derived container
     * back to its source(s). Useful for debugging and visualization.
     * 
     * @param data_key Key to start from
     * @return Vector of data keys from derived to source(s)
     * 
     * @example
     * @code
     * // If "peaks" comes from "areas" which comes from "masks":
     * auto chain = resolver.getLineageChain("peaks");
     * // Returns: ["peaks", "areas", "masks"]
     * @endcode
     */
    [[nodiscard]] std::vector<std::string> getLineageChain(
            std::string const & data_key) const;

    /**
     * @brief Check if a data key has any lineage registered
     * 
     * @param data_key Key to check
     * @return true if lineage is registered for this key
     */
    [[nodiscard]] bool hasLineage(std::string const & data_key) const;

    /**
     * @brief Check if a data key represents source data (no parent)
     * 
     * @param data_key Key to check
     * @return true if this is a source container or has no registered lineage
     */
    [[nodiscard]] bool isSource(std::string const & data_key) const;

private:
    DataManager * _dm;

    // Composition: delegates resolution to the generic LineageResolver
    // These are created lazily when needed
    std::unique_ptr<WhiskerToolbox::Lineage::DataManagerEntityDataSource> _data_source;
    std::unique_ptr<LineageResolver> _resolver;

    /**
     * @brief Ensure the internal resolver is initialized
     * 
     * Creates the DataManagerEntityDataSource and LineageResolver if they
     * don't exist yet. This lazy initialization avoids issues with
     * construction order.
     */
    void ensureResolverInitialized() const;
};

}// namespace WhiskerToolbox::Entity::Lineage

#endif// WHISKERTOOLBOX_ENTITY_RESOLVER_HPP
