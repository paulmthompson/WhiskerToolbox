#ifndef WHISKERTOOLBOX_ENTITY_LINEAGE_RESOLVER_HPP
#define WHISKERTOOLBOX_ENTITY_LINEAGE_RESOLVER_HPP

#include "Entity/EntityTypes.hpp"
#include "Entity/Lineage/LineageRegistry.hpp"
#include "Entity/Lineage/LineageTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <string>
#include <unordered_set>
#include <vector>

namespace WhiskerToolbox::Entity::Lineage {

/**
 * @brief Interface for data-source-specific entity resolution
 *
 * This interface abstracts away the concrete data storage from lineage
 * resolution. Implementations (e.g., DataManagerEntityDataSource) provide
 * the actual data access.
 *
 * By programming to this interface, the lineage system can be tested
 * independently of DataManager and reused with different storage backends.
 *
 * @note Implementations must be thread-safe if the resolver is used from
 *       multiple threads.
 */
class IEntityDataSource {
public:
    virtual ~IEntityDataSource() = default;

    /**
     * @brief Get EntityIds from a container at a specific time and index
     *
     * For containers that store multiple entities per time point (ragged),
     * the local_index selects which entity to return.
     *
     * @param data_key Container identifier
     * @param time Time frame index
     * @param local_index Index within that time (for ragged containers)
     * @return EntityIds found at that location (empty if none/not found)
     */
    [[nodiscard]] virtual std::vector<EntityId> getEntityIds(
            std::string const & data_key,
            TimeFrameIndex time,
            std::size_t local_index) const = 0;

    /**
     * @brief Get ALL EntityIds from a container at a specific time
     *
     * Returns all entity IDs at the given time point, regardless of
     * local index.
     *
     * @param data_key Container identifier
     * @param time Time frame index
     * @return All EntityIds at that time
     */
    [[nodiscard]] virtual std::vector<EntityId> getAllEntityIdsAtTime(
            std::string const & data_key,
            TimeFrameIndex time) const = 0;

    /**
     * @brief Get all EntityIds in a container (across all times)
     *
     * Returns the complete set of entity IDs stored in the container,
     * useful for bulk operations.
     *
     * @param data_key Container identifier
     * @return Set of all EntityIds in the container
     */
    [[nodiscard]] virtual std::unordered_set<EntityId> getAllEntityIds(
            std::string const & data_key) const = 0;

    /**
     * @brief Get the count of elements at a specific time (for iteration)
     *
     * Used to determine how many entities exist at a time point,
     * enabling iteration over all elements.
     *
     * @param data_key Container identifier
     * @param time Time frame index
     * @return Number of elements at that time
     */
    [[nodiscard]] virtual std::size_t getElementCount(
            std::string const & data_key,
            TimeFrameIndex time) const = 0;
};

/**
 * @brief Generic lineage resolver using abstract data source
 *
 * This class implements lineage resolution logic without knowing the
 * concrete data types. It uses an IEntityDataSource to access EntityIds.
 *
 * The resolver supports:
 * - Single-step resolution (derived → immediate source)
 * - Full chain resolution (derived → root sources)
 * - EntityId-based resolution for entity-mapped lineage
 * - Lineage chain queries for visualization/debugging
 *
 * @note The resolver does not own the data source or registry pointers.
 *       The caller must ensure these outlive the resolver.
 *
 * @example
 * @code
 * // Create with mock data source for testing
 * MockEntityDataSource data_source;
 * LineageRegistry registry;
 *
 * // Setup lineage: mask_areas derived from masks
 * registry.setLineage("mask_areas", OneToOneByTime{"masks"});
 *
 * LineageResolver resolver(&data_source, &registry);
 *
 * // Resolve derived element to source
 * auto source_ids = resolver.resolveToSource("mask_areas", TimeFrameIndex(10));
 * @endcode
 */
class LineageResolver {
public:
    /**
     * @brief Construct resolver with data source and registry
     *
     * @param data_source Provides entity lookup (caller owns, must outlive resolver)
     * @param registry Lineage metadata (caller owns, must outlive resolver)
     */
    LineageResolver(IEntityDataSource const * data_source,
                    LineageRegistry const * registry);

    /**
     * @brief Default destructor
     */
    ~LineageResolver() = default;

    // Non-copyable but movable
    LineageResolver(LineageResolver const &) = delete;
    LineageResolver & operator=(LineageResolver const &) = delete;
    LineageResolver(LineageResolver &&) = default;
    LineageResolver & operator=(LineageResolver &&) = default;

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
     * @note For OneToOneByTime, returns EntityIds from source at same time/index
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
     * @note Requires EntityMappedLineage lineage type
     */
    [[nodiscard]] std::vector<EntityId> resolveByEntityId(
            std::string const & data_key,
            EntityId derived_entity_id) const;

    // =========================================================================
    // Query Methods
    // =========================================================================

    /**
     * @brief Get lineage chain (data keys from derived to sources)
     *
     * Returns the sequence of data keys from the derived container
     * back to its source(s). Useful for debugging and visualization.
     *
     * @param data_key Key to start from
     * @return Vector of data keys from derived to source(s)
     */
    [[nodiscard]] std::vector<std::string> getLineageChain(
            std::string const & data_key) const;

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
    IEntityDataSource const * _data_source;
    LineageRegistry const * _registry;

    // =========================================================================
    // Resolution Strategy Dispatch Methods
    // =========================================================================

    [[nodiscard]] std::vector<EntityId> resolveOneToOne(
            OneToOneByTime const & lineage,
            TimeFrameIndex time,
            std::size_t local_index) const;

    [[nodiscard]] std::vector<EntityId> resolveAllToOne(
            AllToOneByTime const & lineage,
            TimeFrameIndex time) const;

    [[nodiscard]] std::vector<EntityId> resolveSubset(
            SubsetLineage const & lineage,
            TimeFrameIndex time,
            std::size_t local_index) const;

    [[nodiscard]] std::vector<EntityId> resolveMultiSource(
            MultiSourceLineage const & lineage,
            TimeFrameIndex time,
            std::size_t local_index) const;

    [[nodiscard]] std::vector<EntityId> resolveExplicit(
            ExplicitLineage const & lineage,
            std::size_t derived_index) const;

    [[nodiscard]] std::vector<EntityId> resolveEntityMapped(
            EntityMappedLineage const & lineage,
            EntityId derived_entity_id) const;

    [[nodiscard]] std::vector<EntityId> resolveImplicit(
            ImplicitEntityMapping const & lineage,
            TimeFrameIndex time,
            std::size_t local_index) const;

    // Helper for resolveToRoot - handles AllToOneByTime specially
    [[nodiscard]] std::vector<EntityId> resolveAllToOneToRoot(
            AllToOneByTime const & lineage,
            TimeFrameIndex time) const;
};

}// namespace WhiskerToolbox::Entity::Lineage

#endif// WHISKERTOOLBOX_ENTITY_LINEAGE_RESOLVER_HPP
