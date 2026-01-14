#include "Lineage/EntityResolver.hpp"
#include "Lineage/DataManagerEntityDataSource.hpp"
#include "DataManager.hpp"
#include "Entity/Lineage/LineageRegistry.hpp"
#include "Entity/Lineage/LineageResolver.hpp"

namespace WhiskerToolbox::Entity::Lineage {

EntityResolver::EntityResolver(DataManager * dm)
    : _dm(dm),
      _data_source(nullptr),
      _resolver(nullptr) {
    // Lazy initialization - don't create resolver until first use
    // This avoids issues with DataManager construction order
}

// Destructor must be defined in the .cpp file where DataManagerEntityDataSource
// and LineageResolver are complete types (due to unique_ptr members)
EntityResolver::~EntityResolver() = default;

void EntityResolver::ensureResolverInitialized() const {
    if (!_dm) {
        return;
    }

    if (!_data_source) {
        // const_cast is safe here because we're only initializing mutable members
        auto * mutable_this = const_cast<EntityResolver *>(this);
        mutable_this->_data_source = std::make_unique<WhiskerToolbox::Lineage::DataManagerEntityDataSource>(_dm);
    }

    if (!_resolver) {
        auto * mutable_this = const_cast<EntityResolver *>(this);
        mutable_this->_resolver = std::make_unique<LineageResolver>(
                _data_source.get(),
                _dm->getLineageRegistry());
    }
}

// =============================================================================
// Time-based Resolution
// =============================================================================

std::vector<EntityId> EntityResolver::resolveToSource(
        std::string const & data_key,
        TimeFrameIndex time,
        std::size_t local_index) const {
    ensureResolverInitialized();
    if (!_resolver) {
        return {};
    }
    return _resolver->resolveToSource(data_key, time, local_index);
}

std::vector<EntityId> EntityResolver::resolveToRoot(
        std::string const & data_key,
        TimeFrameIndex time,
        std::size_t local_index) const {
    ensureResolverInitialized();
    if (!_resolver) {
        return {};
    }
    return _resolver->resolveToRoot(data_key, time, local_index);
}

// =============================================================================
// EntityId-based Resolution
// =============================================================================

std::vector<EntityId> EntityResolver::resolveByEntityId(
        std::string const & data_key,
        EntityId derived_entity_id) const {
    ensureResolverInitialized();
    if (!_resolver) {
        return {};
    }
    return _resolver->resolveByEntityId(data_key, derived_entity_id);
}

std::vector<EntityId> EntityResolver::resolveByEntityIdToRoot(
        std::string const & data_key,
        EntityId derived_entity_id) const {
    // For now, single-step resolution
    // Full chain resolution could be added to LineageResolver if needed
    return resolveByEntityId(data_key, derived_entity_id);
}

// =============================================================================
// Bulk Resolution / Queries
// =============================================================================

std::unordered_set<EntityId> EntityResolver::getAllSourceEntities(
        std::string const & data_key) const {
    ensureResolverInitialized();
    if (!_resolver) {
        return {};
    }
    return _resolver->getAllSourceEntities(data_key);
}

std::vector<std::string> EntityResolver::getLineageChain(
        std::string const & data_key) const {
    ensureResolverInitialized();
    if (!_resolver) {
        return {data_key};
    }
    return _resolver->getLineageChain(data_key);
}

bool EntityResolver::hasLineage(std::string const & data_key) const {
    ensureResolverInitialized();
    if (!_resolver) {
        return false;
    }
    return _resolver->hasLineage(data_key);
}

bool EntityResolver::isSource(std::string const & data_key) const {
    ensureResolverInitialized();
    if (!_resolver) {
        // Without lineage info, assume everything is a source
        return true;
    }
    return _resolver->isSource(data_key);
}

}// namespace WhiskerToolbox::Entity::Lineage
