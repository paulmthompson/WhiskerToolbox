#ifndef WHISKERTOOLBOX_DATAMANAGER_ENTITY_DATA_SOURCE_HPP
#define WHISKERTOOLBOX_DATAMANAGER_ENTITY_DATA_SOURCE_HPP

#include "Entity/Lineage/LineageResolver.hpp"

#include <string>
#include <unordered_set>
#include <vector>

// Forward declarations
class DataManager;

namespace WhiskerToolbox::Lineage {

/**
 * @brief Implements IEntityDataSource using DataManager for data access
 *
 * This adapter bridges the abstract Entity lineage system to the
 * concrete DataManager storage. It contains all the type-specific
 * dispatch logic needed to extract EntityIds from various data types
 * (LineData, MaskData, PointData, DigitalEventSeries, etc.)
 *
 * The type dispatch is contained to this single class, keeping the
 * generic LineageResolver free of DataManager dependencies.
 *
 * @note This class does not own the DataManager pointer. The caller
 *       must ensure the DataManager outlives this object.
 *
 * @example
 * @code
 * DataManager dm;
 * DataManagerEntityDataSource data_source(&dm);
 *
 * // Use with LineageResolver
 * auto* registry = dm.getLineageRegistry();
 * Entity::Lineage::LineageResolver resolver(&data_source, registry);
 *
 * auto ids = resolver.resolveToSource("mask_areas", TimeFrameIndex(10));
 * @endcode
 */
class DataManagerEntityDataSource
    : public WhiskerToolbox::Entity::Lineage::IEntityDataSource {
public:
    /**
     * @brief Construct a data source adapter for a DataManager
     *
     * @param dm Pointer to the DataManager (caller owns, must outlive this object)
     * @note Non-const because DataManager::getData() is not const-qualified
     */
    explicit DataManagerEntityDataSource(DataManager * dm);

    /**
     * @brief Default destructor
     */
    ~DataManagerEntityDataSource() override = default;

    // Non-copyable, non-movable (due to raw pointer dependency)
    DataManagerEntityDataSource(DataManagerEntityDataSource const &) = delete;
    DataManagerEntityDataSource & operator=(DataManagerEntityDataSource const &) = delete;
    DataManagerEntityDataSource(DataManagerEntityDataSource &&) = delete;
    DataManagerEntityDataSource & operator=(DataManagerEntityDataSource &&) = delete;

    /**
     * @brief Get EntityIds from a container at a specific time and index
     *
     * Dispatches to the appropriate data type based on the container's type
     * and extracts EntityIds at the specified time and local index.
     *
     * @param data_key Container identifier
     * @param time Time frame index
     * @param local_index Index within that time (for ragged containers)
     * @return EntityIds found at that location (empty if none/not found)
     */
    [[nodiscard]] std::vector<EntityId> getEntityIds(
            std::string const & data_key,
            TimeFrameIndex time,
            std::size_t local_index) const override;

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
    [[nodiscard]] std::vector<EntityId> getAllEntityIdsAtTime(
            std::string const & data_key,
            TimeFrameIndex time) const override;

    /**
     * @brief Get all EntityIds in a container (across all times)
     *
     * Returns the complete set of entity IDs stored in the container.
     * Dispatches to the appropriate extraction method based on data type.
     *
     * @param data_key Container identifier
     * @return Set of all EntityIds in the container
     */
    [[nodiscard]] std::unordered_set<EntityId> getAllEntityIds(
            std::string const & data_key) const override;

    /**
     * @brief Get the count of elements at a specific time
     *
     * Returns the number of entities at a time point, enabling
     * iteration over all elements for AllToOne resolution.
     *
     * @param data_key Container identifier
     * @param time Time frame index
     * @return Number of elements at that time
     */
    [[nodiscard]] std::size_t getElementCount(
            std::string const & data_key,
            TimeFrameIndex time) const override;

private:
    DataManager * _dm;
};

}// namespace WhiskerToolbox::Lineage

#endif// WHISKERTOOLBOX_DATAMANAGER_ENTITY_DATA_SOURCE_HPP
