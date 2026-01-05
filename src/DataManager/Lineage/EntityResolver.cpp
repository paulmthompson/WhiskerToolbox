#include "Lineage/EntityResolver.hpp"
#include "DataManager.hpp"
#include "Entity/Lineage/LineageRegistry.hpp"

#include "AnalogTimeSeries/RaggedAnalogTimeSeries.hpp"
#include "DigitalTimeSeries/Digital_Event_Series.hpp"
#include "DigitalTimeSeries/Digital_Interval_Series.hpp"
#include "Lines/Line_Data.hpp"
#include "Masks/Mask_Data.hpp"
#include "Points/Point_Data.hpp"

#include <algorithm>
#include <ranges>

namespace WhiskerToolbox::Entity::Lineage {

EntityResolver::EntityResolver(DataManager * dm)
    : _dm(dm) {
}

// =============================================================================
// Time-based Resolution
// =============================================================================

std::vector<EntityId> EntityResolver::resolveToSource(
        std::string const & data_key,
        TimeFrameIndex time,
        std::size_t local_index) const {
    if (!_dm) {
        return {};
    }

    // Get lineage from registry
    auto const * registry = _dm->getLineageRegistry();
    if (!registry) {
        // No lineage registry - just return EntityIds from the container itself
        return getEntityIdsFromContainer(data_key, time, local_index);
    }

    auto lineage_opt = registry->getLineage(data_key);
    if (!lineage_opt) {
        // No lineage registered - return EntityIds from the container itself
        return getEntityIdsFromContainer(data_key, time, local_index);
    }

    // Dispatch based on lineage type
    return std::visit(
            [this, &data_key, &time, local_index](auto const & lineage) -> std::vector<EntityId> {
                using T = std::decay_t<decltype(lineage)>;

                if constexpr (std::is_same_v<T, Source>) {
                    // Source data - return own EntityIds from this container
                    return getEntityIdsFromContainer(data_key, time, local_index);
                } else if constexpr (std::is_same_v<T, OneToOneByTime>) {
                    // 1:1 mapping - get EntityId from source at same time and local_index
                    return getEntityIdsFromContainer(lineage.source_key, time, local_index);
                } else if constexpr (std::is_same_v<T, AllToOneByTime>) {
                    // N:1 mapping - get ALL EntityIds from source at this time
                    return getAllEntityIdsAtTime(lineage.source_key, time);
                } else if constexpr (std::is_same_v<T, SubsetLineage>) {
                    // Subset - get EntityId from source, filtered by included set
                    auto ids = getEntityIdsFromContainer(lineage.source_key, time, local_index);
                    std::vector<EntityId> result;
                    for (auto const & id: ids) {
                        if (lineage.included_entities.count(id) > 0) {
                            result.push_back(id);
                        }
                    }
                    return result;
                } else if constexpr (std::is_same_v<T, MultiSourceLineage>) {
                    // Multi-source - combine EntityIds from all sources
                    std::vector<EntityId> result;
                    for (auto const & source_key: lineage.source_keys) {
                        auto ids = getEntityIdsFromContainer(source_key, time, local_index);
                        result.insert(result.end(), ids.begin(), ids.end());
                    }
                    return result;
                } else if constexpr (std::is_same_v<T, ExplicitLineage>) {
                    // Explicit mapping - look up in contributors map
                    // local_index is the derived index
                    if (local_index < lineage.contributors.size()) {
                        return lineage.contributors[local_index];
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, EntityMappedLineage>) {
                    // Entity-mapped - would need derived EntityId, not position
                    // This path shouldn't be used with position-based resolution
                    return {};
                } else if constexpr (std::is_same_v<T, ImplicitEntityMapping>) {
                    // Implicit mapping by position
                    return getEntityIdsFromContainer(lineage.source_key, time, local_index);
                } else {
                    return {};
                }
            },
            lineage_opt.value());
}

std::vector<EntityId> EntityResolver::resolveToRoot(
        std::string const & data_key,
        TimeFrameIndex time,
        std::size_t local_index) const {
    if (!_dm) {
        return {};
    }

    auto const * registry = _dm->getLineageRegistry();
    if (!registry) {
        return getEntityIdsFromContainer(data_key, time, local_index);
    }

    // Check if this key has lineage
    auto lineage_opt = registry->getLineage(data_key);
    if (!lineage_opt) {
        // No lineage - return EntityIds from this container
        return getEntityIdsFromContainer(data_key, time, local_index);
    }

    // Handle Source specially - it means this is a root/terminal node
    if (std::holds_alternative<Source>(lineage_opt.value())) {
        return getEntityIdsFromContainer(data_key, time, local_index);
    }

    // Recursively resolve based on lineage type
    return std::visit(
            [this, &time, local_index](auto const & lineage) -> std::vector<EntityId> {
                using T = std::decay_t<decltype(lineage)>;

                if constexpr (std::is_same_v<T, Source>) {
                    // Should not reach here - handled above
                    return {};
                } else if constexpr (std::is_same_v<T, OneToOneByTime>) {
                    // 1:1 mapping - resolve recursively through source
                    return resolveToRoot(lineage.source_key, time, local_index);
                } else if constexpr (std::is_same_v<T, AllToOneByTime>) {
                    // N:1 mapping - need to resolve ALL elements at this time through source
                    // First check if source has its own lineage
                    auto const * reg = _dm->getLineageRegistry();
                    auto source_lineage = reg ? reg->getLineage(lineage.source_key) : std::nullopt;
                    
                    if (!source_lineage || std::holds_alternative<Source>(source_lineage.value())) {
                        // Source is terminal - get all EntityIds directly
                        return getAllEntityIdsAtTime(lineage.source_key, time);
                    }
                    
                    // Source has lineage - we need to resolve all elements at this time through it
                    // Get count of elements at this time in the source
                    std::vector<EntityId> all_root_ids;
                    
                    // We need to figure out how many elements are at this time in the intermediate container
                    // and resolve each one to root
                    DM_DataType const type = _dm->getType(lineage.source_key);
                    std::size_t count = 0;
                    
                    // Determine count based on data type
                    switch (type) {
                        case DM_DataType::RaggedAnalog:
                            if (auto data = _dm->getData<RaggedAnalogTimeSeries>(lineage.source_key)) {
                                auto values = data->getDataAtTime(time);
                                count = values.size();
                            }
                            break;
                        case DM_DataType::Mask:
                            if (auto data = _dm->getData<MaskData>(lineage.source_key)) {
                                count = data->getEntityIdsAtTime(time).size();
                            }
                            break;
                        case DM_DataType::Line:
                            if (auto data = _dm->getData<LineData>(lineage.source_key)) {
                                count = data->getEntityIdsAtTime(time).size();
                            }
                            break;
                        case DM_DataType::Points:
                            if (auto data = _dm->getData<PointData>(lineage.source_key)) {
                                count = data->getEntityIdsAtTime(time).size();
                            }
                            break;
                        default:
                            // For other types, try with local_index 0
                            count = 1;
                            break;
                    }
                    
                    // Resolve each element at this time through the chain
                    for (std::size_t i = 0; i < count; ++i) {
                        auto ids = resolveToRoot(lineage.source_key, time, i);
                        all_root_ids.insert(all_root_ids.end(), ids.begin(), ids.end());
                    }
                    
                    return all_root_ids;
                } else if constexpr (std::is_same_v<T, SubsetLineage>) {
                    // Subset - resolve through source, filtered
                    auto ids = resolveToRoot(lineage.source_key, time, local_index);
                    std::vector<EntityId> result;
                    for (auto const & id: ids) {
                        if (lineage.included_entities.count(id) > 0) {
                            result.push_back(id);
                        }
                    }
                    return result;
                } else if constexpr (std::is_same_v<T, MultiSourceLineage>) {
                    // Multi-source - resolve through all sources
                    std::vector<EntityId> result;
                    for (auto const & source_key: lineage.source_keys) {
                        auto ids = resolveToRoot(source_key, time, local_index);
                        result.insert(result.end(), ids.begin(), ids.end());
                    }
                    return result;
                } else if constexpr (std::is_same_v<T, ExplicitLineage>) {
                    // Explicit - already at root level (contributors are EntityIds)
                    if (local_index < lineage.contributors.size()) {
                        return lineage.contributors[local_index];
                    }
                    return {};
                } else if constexpr (std::is_same_v<T, ImplicitEntityMapping>) {
                    return resolveToRoot(lineage.source_key, time, local_index);
                } else if constexpr (std::is_same_v<T, EntityMappedLineage>) {
                    // Would need derived EntityId, not local_index
                    return {};
                } else {
                    return {};
                }
            },
            lineage_opt.value());
}

// =============================================================================
// EntityId-based Resolution
// =============================================================================

std::vector<EntityId> EntityResolver::resolveByEntityId(
        std::string const & data_key,
        EntityId derived_entity_id) const {
    if (!_dm) {
        return {};
    }

    // This requires EntityMappedLineage or ImplicitEntityMapping
    // Will be fully implemented with LineageRegistry integration
    return {};
}

std::vector<EntityId> EntityResolver::resolveByEntityIdToRoot(
        std::string const & data_key,
        EntityId derived_entity_id) const {
    if (!_dm) {
        return {};
    }

    // Chain resolution from EntityId
    // Will be fully implemented with LineageRegistry integration
    return resolveByEntityId(data_key, derived_entity_id);
}

// =============================================================================
// Bulk Resolution / Queries
// =============================================================================

namespace {

// Helper to extract all EntityIds from a RaggedTimeSeries using flattened_data()
template<typename T>
std::unordered_set<EntityId> extractEntityIdsFromRagged(std::shared_ptr<T> const & data) {
    std::unordered_set<EntityId> result;
    for (auto const & [time, entity_id, data_ref]: data->flattened_data()) {
        result.insert(entity_id);
    }
    return result;
}

// Helper to extract all EntityIds from types with getEntityIds() method
template<typename T>
std::unordered_set<EntityId> extractEntityIdsFromVector(std::shared_ptr<T> const & data) {
    auto const & ids = data->getEntityIds();
    return std::unordered_set<EntityId>(ids.begin(), ids.end());
}

}// anonymous namespace

std::unordered_set<EntityId> EntityResolver::getAllSourceEntities(
        std::string const & data_key) const {
    if (!_dm) {
        return {};
    }

    // Use getType() to dispatch based on data type enum
    DM_DataType const type = _dm->getType(data_key);

    switch (type) {
        case DM_DataType::Line:
            if (auto data = _dm->getData<LineData>(data_key)) {
                return extractEntityIdsFromRagged(data);
            }
            break;

        case DM_DataType::Mask:
            if (auto data = _dm->getData<MaskData>(data_key)) {
                return extractEntityIdsFromRagged(data);
            }
            break;

        case DM_DataType::Points:
            if (auto data = _dm->getData<PointData>(data_key)) {
                return extractEntityIdsFromRagged(data);
            }
            break;

        case DM_DataType::DigitalEvent:
            if (auto data = _dm->getData<DigitalEventSeries>(data_key)) {
                return extractEntityIdsFromVector(data);
            }
            break;

        case DM_DataType::DigitalInterval:
            if (auto data = _dm->getData<DigitalIntervalSeries>(data_key)) {
                return extractEntityIdsFromVector(data);
            }
            break;

        // Types without EntityIds
        case DM_DataType::Video:
        case DM_DataType::Images:
        case DM_DataType::Analog:
        case DM_DataType::RaggedAnalog:
        case DM_DataType::Tensor:
        case DM_DataType::Time:
        case DM_DataType::Unknown:
            break;
    }

    return {};
}

std::vector<std::string> EntityResolver::getLineageChain(
        std::string const & data_key) const {
    // Delegate to LineageRegistry if available
    auto const * registry = _dm ? _dm->getLineageRegistry() : nullptr;
    if (registry) {
        return registry->getLineageChain(data_key);
    }
    return {data_key};
}

bool EntityResolver::hasLineage(std::string const & data_key) const {
    auto const * registry = _dm ? _dm->getLineageRegistry() : nullptr;
    if (registry) {
        return registry->hasLineage(data_key);
    }
    return false;
}

bool EntityResolver::isSource(std::string const & data_key) const {
    auto const * registry = _dm ? _dm->getLineageRegistry() : nullptr;
    if (registry) {
        return registry->isSource(data_key);
    }
    // Without lineage info, assume everything is a source
    return true;
}

// =============================================================================
// Private Helper Methods
// =============================================================================

namespace {

// Helper to get EntityId at a specific local_index for RaggedTimeSeries types
template<typename T>
std::optional<EntityId> getEntityIdAtLocalIndex(
        std::shared_ptr<T> const & data,
        TimeFrameIndex time,
        std::size_t local_index) {
    auto ids_range = data->getEntityIdsAtTime(time);
    std::size_t idx = 0;
    for (auto const id: ids_range) {
        if (idx == local_index) {
            return id;
        }
        ++idx;
    }
    return std::nullopt;
}

// Helper to collect all EntityIds at a time for RaggedTimeSeries
template<typename T>
std::vector<EntityId> collectEntityIdsAtTime(
        std::shared_ptr<T> const & data,
        TimeFrameIndex time) {
    std::vector<EntityId> result;
    for (auto const id: data->getEntityIdsAtTime(time)) {
        result.push_back(id);
    }
    return result;
}

}// anonymous namespace

std::vector<EntityId> EntityResolver::getEntityIdsFromContainer(
        std::string const & data_key,
        TimeFrameIndex time,
        std::size_t local_index) const {

    DM_DataType const type = _dm->getType(data_key);

    switch (type) {
        case DM_DataType::Line:
            if (auto data = _dm->getData<LineData>(data_key)) {
                if (auto id = getEntityIdAtLocalIndex(data, time, local_index)) {
                    return {*id};
                }
            }
            break;

        case DM_DataType::Mask:
            if (auto data = _dm->getData<MaskData>(data_key)) {
                if (auto id = getEntityIdAtLocalIndex(data, time, local_index)) {
                    return {*id};
                }
            }
            break;

        case DM_DataType::Points:
            if (auto data = _dm->getData<PointData>(data_key)) {
                if (auto id = getEntityIdAtLocalIndex(data, time, local_index)) {
                    return {*id};
                }
            }
            break;

        case DM_DataType::DigitalEvent:
            if (auto data = _dm->getData<DigitalEventSeries>(data_key)) {
                auto const & events = data->getEventSeries();
                auto const & entity_ids = data->getEntityIds();
                for (std::size_t i = 0; i < events.size(); ++i) {
                    if (events[i] == time && i < entity_ids.size()) {
                        return {entity_ids[i]};
                    }
                }
            }
            break;

        case DM_DataType::DigitalInterval:
            if (auto data = _dm->getData<DigitalIntervalSeries>(data_key)) {
                auto const & intervals = data->getDigitalIntervalSeries();
                auto const & entity_ids = data->getEntityIds();
                for (std::size_t i = 0; i < intervals.size(); ++i) {
                    if (TimeFrameIndex(intervals[i].start) <= time &&
                        time <= TimeFrameIndex(intervals[i].end) &&
                        i < entity_ids.size()) {
                        return {entity_ids[i]};
                    }
                }
            }
            break;

        case DM_DataType::Video:
        case DM_DataType::Images:
        case DM_DataType::Analog:
        case DM_DataType::RaggedAnalog:
        case DM_DataType::Tensor:
        case DM_DataType::Time:
        case DM_DataType::Unknown:
            break;
    }

    return {};
}

std::vector<EntityId> EntityResolver::getAllEntityIdsAtTime(
        std::string const & data_key,
        TimeFrameIndex time) const {

    DM_DataType const type = _dm->getType(data_key);

    switch (type) {
        case DM_DataType::Line:
            if (auto data = _dm->getData<LineData>(data_key)) {
                return collectEntityIdsAtTime(data, time);
            }
            break;

        case DM_DataType::Mask:
            if (auto data = _dm->getData<MaskData>(data_key)) {
                return collectEntityIdsAtTime(data, time);
            }
            break;

        case DM_DataType::Points:
            if (auto data = _dm->getData<PointData>(data_key)) {
                return collectEntityIdsAtTime(data, time);
            }
            break;

        case DM_DataType::DigitalEvent:
            if (auto data = _dm->getData<DigitalEventSeries>(data_key)) {
                std::vector<EntityId> result;
                auto const & events = data->getEventSeries();
                auto const & entity_ids = data->getEntityIds();
                for (std::size_t i = 0; i < events.size(); ++i) {
                    if (events[i] == time && i < entity_ids.size()) {
                        result.push_back(entity_ids[i]);
                    }
                }
                return result;
            }
            break;

        case DM_DataType::DigitalInterval:
            if (auto data = _dm->getData<DigitalIntervalSeries>(data_key)) {
                std::vector<EntityId> result;
                auto const & intervals = data->getDigitalIntervalSeries();
                auto const & entity_ids = data->getEntityIds();
                for (std::size_t i = 0; i < intervals.size(); ++i) {
                    if (TimeFrameIndex(intervals[i].start) <= time &&
                        time <= TimeFrameIndex(intervals[i].end) &&
                        i < entity_ids.size()) {
                        result.push_back(entity_ids[i]);
                    }
                }
                return result;
            }
            break;

        case DM_DataType::Video:
        case DM_DataType::Images:
        case DM_DataType::Analog:
        case DM_DataType::RaggedAnalog:
        case DM_DataType::Tensor:
        case DM_DataType::Time:
        case DM_DataType::Unknown:
            break;
    }

    return {};
}

// =============================================================================
// Resolution Strategy Implementations
// =============================================================================

std::vector<EntityId> EntityResolver::resolveOneToOne(
        OneToOneByTime const & lineage,
        TimeFrameIndex time,
        std::size_t local_index) const {
    return getEntityIdsFromContainer(lineage.source_key, time, local_index);
}

std::vector<EntityId> EntityResolver::resolveAllToOne(
        AllToOneByTime const & lineage,
        TimeFrameIndex time) const {
    return getAllEntityIdsAtTime(lineage.source_key, time);
}

std::vector<EntityId> EntityResolver::resolveSubset(
        SubsetLineage const & lineage,
        TimeFrameIndex time) const {
    // Get all EntityIds at time from source, filter by included set
    auto all_ids = getAllEntityIdsAtTime(lineage.source_key, time);

    std::vector<EntityId> result;
    for (auto const & id: all_ids) {
        if (lineage.included_entities.count(id) > 0) {
            result.push_back(id);
        }
    }

    return result;
}

std::vector<EntityId> EntityResolver::resolveMultiSource(
        MultiSourceLineage const & lineage,
        TimeFrameIndex time,
        std::size_t local_index) const {
    std::vector<EntityId> result;

    switch (lineage.strategy) {
        case MultiSourceLineage::CombineStrategy::ZipByTime:
            // Collect EntityIds from all sources at this time
            for (auto const & source_key: lineage.source_keys) {
                auto ids = getAllEntityIdsAtTime(source_key, time);
                result.insert(result.end(), ids.begin(), ids.end());
            }
            break;

        case MultiSourceLineage::CombineStrategy::Cartesian:
        case MultiSourceLineage::CombineStrategy::Custom:
            // These would need more complex handling
            // For now, fall back to collecting all
            for (auto const & source_key: lineage.source_keys) {
                auto ids = getAllEntityIdsAtTime(source_key, time);
                result.insert(result.end(), ids.begin(), ids.end());
            }
            break;
    }

    return result;
}

std::vector<EntityId> EntityResolver::resolveExplicit(
        ExplicitLineage const & lineage,
        std::size_t derived_index) const {
    if (derived_index < lineage.contributors.size()) {
        return lineage.contributors[derived_index];
    }
    return {};
}

std::vector<EntityId> EntityResolver::resolveEntityMapped(
        EntityMappedLineage const & lineage,
        EntityId derived_entity_id) const {
    auto it = lineage.entity_mapping.find(derived_entity_id);
    if (it != lineage.entity_mapping.end()) {
        return it->second;
    }
    return {};
}

std::vector<EntityId> EntityResolver::resolveImplicit(
        ImplicitEntityMapping const & lineage,
        TimeFrameIndex time,
        std::size_t local_index) const {
    switch (lineage.cardinality) {
        case ImplicitEntityMapping::Cardinality::OneToOne:
            // Same position in source
            return getEntityIdsFromContainer(lineage.source_key, time, local_index);

        case ImplicitEntityMapping::Cardinality::AllToOne:
            // All source entities at this time
            return getAllEntityIdsAtTime(lineage.source_key, time);

        case ImplicitEntityMapping::Cardinality::OneToAll:
            // First source entity at this time
            return getEntityIdsFromContainer(lineage.source_key, time, 0);
    }

    return {};
}

}// namespace WhiskerToolbox::Entity::Lineage
