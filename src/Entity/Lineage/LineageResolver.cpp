#include "Entity/Lineage/LineageResolver.hpp"

#include <algorithm>

namespace WhiskerToolbox::Entity::Lineage {

LineageResolver::LineageResolver(IEntityDataSource const * data_source,
                                 LineageRegistry const * registry)
    : _data_source(data_source),
      _registry(registry) {
}

// =============================================================================
// Time-based Resolution
// =============================================================================

std::vector<EntityId> LineageResolver::resolveToSource(
        std::string const & data_key,
        TimeFrameIndex time,
        std::size_t local_index) const {
    if (!_data_source) {
        return {};
    }

    // If no registry or no lineage, return EntityIds from the container itself
    if (!_registry) {
        return _data_source->getEntityIds(data_key, time, local_index);
    }

    auto lineage_opt = _registry->getLineage(data_key);
    if (!lineage_opt) {
        // No lineage registered - return EntityIds from the container itself
        return _data_source->getEntityIds(data_key, time, local_index);
    }

    // Dispatch based on lineage type
    return std::visit(
            [this, &data_key, &time, local_index](auto const & lineage) -> std::vector<EntityId> {
                using T = std::decay_t<decltype(lineage)>;

                if constexpr (std::is_same_v<T, Source>) {
                    // Source data - return own EntityIds from this container
                    return _data_source->getEntityIds(data_key, time, local_index);
                } else if constexpr (std::is_same_v<T, OneToOneByTime>) {
                    return resolveOneToOne(lineage, time, local_index);
                } else if constexpr (std::is_same_v<T, AllToOneByTime>) {
                    return resolveAllToOne(lineage, time);
                } else if constexpr (std::is_same_v<T, SubsetLineage>) {
                    return resolveSubset(lineage, time, local_index);
                } else if constexpr (std::is_same_v<T, MultiSourceLineage>) {
                    return resolveMultiSource(lineage, time, local_index);
                } else if constexpr (std::is_same_v<T, ExplicitLineage>) {
                    return resolveExplicit(lineage, local_index);
                } else if constexpr (std::is_same_v<T, EntityMappedLineage>) {
                    // Entity-mapped requires EntityId, not position
                    // This path shouldn't be used with position-based resolution
                    return {};
                } else if constexpr (std::is_same_v<T, ImplicitEntityMapping>) {
                    return resolveImplicit(lineage, time, local_index);
                } else {
                    return {};
                }
            },
            lineage_opt.value());
}

std::vector<EntityId> LineageResolver::resolveToRoot(
        std::string const & data_key,
        TimeFrameIndex time,
        std::size_t local_index) const {
    if (!_data_source) {
        return {};
    }

    if (!_registry) {
        return _data_source->getEntityIds(data_key, time, local_index);
    }

    // Check if this key has lineage
    auto lineage_opt = _registry->getLineage(data_key);
    if (!lineage_opt) {
        // No lineage - return EntityIds from this container
        return _data_source->getEntityIds(data_key, time, local_index);
    }

    // Handle Source specially - it means this is a root/terminal node
    if (std::holds_alternative<Source>(lineage_opt.value())) {
        return _data_source->getEntityIds(data_key, time, local_index);
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
                    return resolveAllToOneToRoot(lineage, time);
                } else if constexpr (std::is_same_v<T, SubsetLineage>) {
                    // Subset - resolve through source, filtered
                    auto ids = resolveToRoot(lineage.source_key, time, local_index);
                    std::vector<EntityId> result;
                    for (auto const & id : ids) {
                        if (lineage.included_entities.count(id) > 0) {
                            result.push_back(id);
                        }
                    }
                    return result;
                } else if constexpr (std::is_same_v<T, MultiSourceLineage>) {
                    // Multi-source - resolve through all sources
                    std::vector<EntityId> result;
                    for (auto const & source_key : lineage.source_keys) {
                        auto ids = resolveToRoot(source_key, time, local_index);
                        result.insert(result.end(), ids.begin(), ids.end());
                    }
                    return result;
                } else if constexpr (std::is_same_v<T, ExplicitLineage>) {
                    // Explicit - contributors are already EntityIds (assumed to be root)
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

std::vector<EntityId> LineageResolver::resolveByEntityId(
        std::string const & data_key,
        EntityId derived_entity_id) const {
    if (!_registry) {
        return {};
    }

    auto lineage_opt = _registry->getLineage(data_key);
    if (!lineage_opt) {
        return {};
    }

    // Only EntityMappedLineage supports EntityId-based resolution
    if (auto * mapped = std::get_if<EntityMappedLineage>(&lineage_opt.value())) {
        return resolveEntityMapped(*mapped, derived_entity_id);
    }

    return {};
}

// =============================================================================
// Query Methods
// =============================================================================

std::vector<std::string> LineageResolver::getLineageChain(
        std::string const & data_key) const {
    if (_registry) {
        return _registry->getLineageChain(data_key);
    }
    return {data_key};
}

std::unordered_set<EntityId> LineageResolver::getAllSourceEntities(
        std::string const & data_key) const {
    if (!_registry) {
        // No registry - get all entities from the container itself
        if (_data_source) {
            return _data_source->getAllEntityIds(data_key);
        }
        return {};
    }

    auto lineage_opt = _registry->getLineage(data_key);
    if (!lineage_opt || std::holds_alternative<Source>(lineage_opt.value())) {
        // No lineage or is source - get entities from container
        if (_data_source) {
            return _data_source->getAllEntityIds(data_key);
        }
        return {};
    }

    // Get source keys and collect all their entities
    auto source_keys = getSourceKeys(lineage_opt.value());
    std::unordered_set<EntityId> result;

    for (auto const & source_key : source_keys) {
        if (_data_source) {
            auto source_entities = _data_source->getAllEntityIds(source_key);
            result.insert(source_entities.begin(), source_entities.end());
        }
    }

    return result;
}

bool LineageResolver::hasLineage(std::string const & data_key) const {
    if (_registry) {
        return _registry->hasLineage(data_key);
    }
    return false;
}

bool LineageResolver::isSource(std::string const & data_key) const {
    if (_registry) {
        return _registry->isSource(data_key);
    }
    // Without lineage info, assume everything is a source
    return true;
}

// =============================================================================
// Resolution Strategy Implementations
// =============================================================================

std::vector<EntityId> LineageResolver::resolveOneToOne(
        OneToOneByTime const & lineage,
        TimeFrameIndex time,
        std::size_t local_index) const {
    return _data_source->getEntityIds(lineage.source_key, time, local_index);
}

std::vector<EntityId> LineageResolver::resolveAllToOne(
        AllToOneByTime const & lineage,
        TimeFrameIndex time) const {
    return _data_source->getAllEntityIdsAtTime(lineage.source_key, time);
}

std::vector<EntityId> LineageResolver::resolveSubset(
        SubsetLineage const & lineage,
        TimeFrameIndex time,
        std::size_t local_index) const {
    // Get EntityId from source at the given position
    auto ids = _data_source->getEntityIds(lineage.source_key, time, local_index);

    // Filter by included set
    std::vector<EntityId> result;
    for (auto const & id : ids) {
        if (lineage.included_entities.count(id) > 0) {
            result.push_back(id);
        }
    }

    return result;
}

std::vector<EntityId> LineageResolver::resolveMultiSource(
        MultiSourceLineage const & lineage,
        TimeFrameIndex time,
        std::size_t local_index) const {
    std::vector<EntityId> result;

    switch (lineage.strategy) {
        case MultiSourceLineage::CombineStrategy::ZipByTime:
            // Collect EntityIds from all sources at this time
            for (auto const & source_key : lineage.source_keys) {
                auto ids = _data_source->getAllEntityIdsAtTime(source_key, time);
                result.insert(result.end(), ids.begin(), ids.end());
            }
            break;

        case MultiSourceLineage::CombineStrategy::Cartesian:
        case MultiSourceLineage::CombineStrategy::Custom:
            // For these strategies, fall back to collecting all
            for (auto const & source_key : lineage.source_keys) {
                auto ids = _data_source->getAllEntityIdsAtTime(source_key, time);
                result.insert(result.end(), ids.begin(), ids.end());
            }
            break;
    }

    return result;
}

std::vector<EntityId> LineageResolver::resolveExplicit(
        ExplicitLineage const & lineage,
        std::size_t derived_index) const {
    if (derived_index < lineage.contributors.size()) {
        return lineage.contributors[derived_index];
    }
    return {};
}

std::vector<EntityId> LineageResolver::resolveEntityMapped(
        EntityMappedLineage const & lineage,
        EntityId derived_entity_id) const {
    auto it = lineage.entity_mapping.find(derived_entity_id);
    if (it != lineage.entity_mapping.end()) {
        return it->second;
    }
    return {};
}

std::vector<EntityId> LineageResolver::resolveImplicit(
        ImplicitEntityMapping const & lineage,
        TimeFrameIndex time,
        std::size_t local_index) const {
    switch (lineage.cardinality) {
        case ImplicitEntityMapping::Cardinality::OneToOne:
            // Same position in source
            return _data_source->getEntityIds(lineage.source_key, time, local_index);

        case ImplicitEntityMapping::Cardinality::AllToOne:
            // All source entities at this time
            return _data_source->getAllEntityIdsAtTime(lineage.source_key, time);

        case ImplicitEntityMapping::Cardinality::OneToAll:
            // First source entity at this time
            return _data_source->getEntityIds(lineage.source_key, time, 0);
    }

    return {};
}

std::vector<EntityId> LineageResolver::resolveAllToOneToRoot(
        AllToOneByTime const & lineage,
        TimeFrameIndex time) const {
    // Check if source has its own lineage
    auto source_lineage = _registry->getLineage(lineage.source_key);

    if (!source_lineage || std::holds_alternative<Source>(source_lineage.value())) {
        // Source is terminal - get all EntityIds directly
        return _data_source->getAllEntityIdsAtTime(lineage.source_key, time);
    }

    // Source has lineage - we need to resolve all elements at this time through it
    // Get count of elements at this time in the source
    std::size_t count = _data_source->getElementCount(lineage.source_key, time);

    std::vector<EntityId> all_root_ids;

    // Resolve each element at this time through the chain
    for (std::size_t i = 0; i < count; ++i) {
        auto ids = resolveToRoot(lineage.source_key, time, i);
        all_root_ids.insert(all_root_ids.end(), ids.begin(), ids.end());
    }

    return all_root_ids;
}

}// namespace WhiskerToolbox::Entity::Lineage
