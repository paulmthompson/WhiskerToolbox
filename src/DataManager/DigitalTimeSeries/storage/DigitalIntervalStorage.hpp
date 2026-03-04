#ifndef DIGITAL_INTERVAL_STORAGE_HPP
#define DIGITAL_INTERVAL_STORAGE_HPP

#include "DigitalIntervalStorageBase.hpp"
#include "DigitalIntervalStorageCache.hpp"
#include "LazyDigitalIntervalStorage.hpp"

#include "Entity/EntityTypes.hpp"
//#include "TimeFrame/TimeFrame.hpp"
#include "TimeFrame/interval_data.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

/**
 * @brief Hash specialization for Interval to allow use in unordered containers
 */
template<>
struct std::hash<Interval> {
    size_t operator()(Interval const& interval) const noexcept {
        // Combine start and end using a simple hash combination
        size_t h1 = std::hash<int64_t>{}(interval.start);
        size_t h2 = std::hash<int64_t>{}(interval.end);
        return h1 ^ (h2 << 1);
    }
};

class OwningDigitalIntervalStorage;
class ViewDigitalIntervalStorage;



// =============================================================================
// Type-Erased Storage Wrapper (Virtual Dispatch)
// =============================================================================

/**
 * @brief Type-erased storage wrapper for digital interval storage
 * 
 * Provides a uniform interface for any storage backend while hiding the
 * concrete storage type. Supports lazy transforms with unbounded ViewType.
 */
class DigitalIntervalStorageWrapper {
public:
    /**
     * @brief Construct wrapper from any storage implementation
     */
    template<typename StorageImpl>
    explicit DigitalIntervalStorageWrapper(StorageImpl storage)
        : _impl(std::make_shared<StorageModel<StorageImpl>>(std::move(storage))) {}

    // Default constructor creates empty owning storage
    DigitalIntervalStorageWrapper();

    // Copy and move semantics - shared_ptr allows sharing
    DigitalIntervalStorageWrapper(DigitalIntervalStorageWrapper&&) noexcept = default;
    DigitalIntervalStorageWrapper& operator=(DigitalIntervalStorageWrapper&&) noexcept = default;
    DigitalIntervalStorageWrapper(DigitalIntervalStorageWrapper const&) = default;
    DigitalIntervalStorageWrapper& operator=(DigitalIntervalStorageWrapper const&) = default;

    // ========== Unified Interface ==========
    
    [[nodiscard]] size_t size() const { return _impl->size(); }
    [[nodiscard]] bool empty() const { return _impl->size() == 0; }

    [[nodiscard]] Interval const& getInterval(size_t idx) const {
        return _impl->getInterval(idx);
    }

    [[nodiscard]] EntityId getEntityId(size_t idx) const {
        return _impl->getEntityId(idx);
    }

    [[nodiscard]] std::optional<size_t> findByInterval(Interval const& interval) const {
        return _impl->findByInterval(interval);
    }
    
    [[nodiscard]] std::optional<size_t> findByEntityId(EntityId id) const {
        return _impl->findByEntityId(id);
    }
    
    [[nodiscard]] bool hasIntervalAtTime(int64_t time) const {
        return _impl->hasIntervalAtTime(time);
    }

    [[nodiscard]] std::pair<size_t, size_t> getOverlappingRange(int64_t start, int64_t end) const {
        return _impl->getOverlappingRange(start, end);
    }
    
    [[nodiscard]] std::pair<size_t, size_t> getContainedRange(int64_t start, int64_t end) const {
        return _impl->getContainedRange(start, end);
    }

    [[nodiscard]] DigitalIntervalStorageType getStorageType() const {
        return _impl->getStorageType();
    }

    [[nodiscard]] bool isView() const {
        return getStorageType() == DigitalIntervalStorageType::View;
    }
    
    [[nodiscard]] bool isLazy() const {
        return getStorageType() == DigitalIntervalStorageType::Lazy;
    }

    // ========== Cache Optimization ==========
    
    [[nodiscard]] DigitalIntervalStorageCache tryGetCache() const {
        return _impl->tryGetCache();
    }

    // ========== Mutation Operations ==========
    
    bool addInterval(Interval const& interval, EntityId entity_id) {
        return _impl->addInterval(interval, entity_id);
    }

    bool removeInterval(Interval const& interval) {
        return _impl->removeInterval(interval);
    }
    
    bool removeByEntityId(EntityId id) {
        return _impl->removeByEntityId(id);
    }

    void reserve(size_t capacity) {
        _impl->reserve(capacity);
    }

    void clear() {
        _impl->clear();
    }
    
    void setEntityIds(std::vector<EntityId> ids) {
        _impl->setEntityIds(std::move(ids));
    }

    // ========== Type Access ==========
    
    template<typename StorageType>
    [[nodiscard]] StorageType* tryGet() {
        auto* model = dynamic_cast<StorageModel<StorageType>*>(_impl.get());
        return model ? &model->_storage : nullptr;
    }

    template<typename StorageType>
    [[nodiscard]] StorageType const* tryGet() const {
        auto const* model = dynamic_cast<StorageModel<StorageType> const*>(_impl.get());
        return model ? &model->_storage : nullptr;
    }
    
    /**
     * @brief Try to get mutable owning storage
     */
    [[nodiscard]] OwningDigitalIntervalStorage* tryGetMutableOwning();
    
    [[nodiscard]] OwningDigitalIntervalStorage const* tryGetOwning() const;

    /**
     * @brief Get shared pointer to owning storage for creating views
     * 
     * If this wrapper contains owning storage, uses the aliasing constructor
     * to create a shared_ptr that shares ownership with the wrapper but points
     * to the inner storage. This enables zero-copy view creation while keeping
     * the source data alive.
     * 
     * If this wrapper contains a view, returns the view's existing source 
     * (which is already a shared_ptr).
     * 
     * Returns nullptr for lazy storage.
     * 
     * @return shared_ptr to owning storage, or nullptr if not available
     */
    [[nodiscard]] std::shared_ptr<OwningDigitalIntervalStorage const> getSharedOwningStorage() const;

private:
    struct StorageConcept {
        virtual ~StorageConcept() = default;
        
        virtual size_t size() const = 0;
        virtual Interval const& getInterval(size_t idx) const = 0;
        virtual EntityId getEntityId(size_t idx) const = 0;
        virtual std::optional<size_t> findByInterval(Interval const& interval) const = 0;
        virtual std::optional<size_t> findByEntityId(EntityId id) const = 0;
        virtual bool hasIntervalAtTime(int64_t time) const = 0;
        virtual std::pair<size_t, size_t> getOverlappingRange(int64_t start, int64_t end) const = 0;
        virtual std::pair<size_t, size_t> getContainedRange(int64_t start, int64_t end) const = 0;
        virtual DigitalIntervalStorageType getStorageType() const = 0;
        virtual DigitalIntervalStorageCache tryGetCache() const = 0;
        
        // Mutation
        virtual bool addInterval(Interval const& interval, EntityId entity_id) = 0;
        virtual bool removeInterval(Interval const& interval) = 0;
        virtual bool removeByEntityId(EntityId id) = 0;
        virtual void reserve(size_t capacity) = 0;
        virtual void clear() = 0;
        virtual void setEntityIds(std::vector<EntityId> ids) = 0;
    };

    template<typename StorageImpl>
    struct StorageModel : StorageConcept {
        StorageImpl _storage;

        explicit StorageModel(StorageImpl storage)
            : _storage(std::move(storage)) {}

        size_t size() const override { return _storage.size(); }
        
        Interval const& getInterval(size_t idx) const override {
            return _storage.getInterval(idx);
        }
        
        EntityId getEntityId(size_t idx) const override {
            return _storage.getEntityId(idx);
        }
        
        std::optional<size_t> findByInterval(Interval const& interval) const override {
            return _storage.findByInterval(interval);
        }
        
        std::optional<size_t> findByEntityId(EntityId id) const override {
            return _storage.findByEntityId(id);
        }
        
        bool hasIntervalAtTime(int64_t time) const override {
            return _storage.hasIntervalAtTime(time);
        }
        
        std::pair<size_t, size_t> getOverlappingRange(int64_t start, int64_t end) const override {
            return _storage.getOverlappingRange(start, end);
        }
        
        std::pair<size_t, size_t> getContainedRange(int64_t start, int64_t end) const override {
            return _storage.getContainedRange(start, end);
        }
        
        DigitalIntervalStorageType getStorageType() const override {
            return _storage.getStorageType();
        }
        
        DigitalIntervalStorageCache tryGetCache() const override {
            if constexpr (requires { _storage.tryGetCache(); }) {
                return _storage.tryGetCache();
            } else {
                return DigitalIntervalStorageCache{};
            }
        }

        // Mutation - only for OwningDigitalIntervalStorage
        bool addInterval(Interval const& interval, EntityId entity_id) override {
            if constexpr (requires { _storage.addInterval(interval, entity_id); }) {
                return _storage.addInterval(interval, entity_id);
            } else {
                throw std::runtime_error("addInterval() not supported for view/lazy storage");
            }
        }
        
        bool removeInterval(Interval const& interval) override {
            if constexpr (requires { _storage.removeInterval(interval); }) {
                return _storage.removeInterval(interval);
            } else {
                throw std::runtime_error("removeInterval() not supported for view/lazy storage");
            }
        }
        
        bool removeByEntityId(EntityId id) override {
            if constexpr (requires { _storage.removeByEntityId(id); }) {
                return _storage.removeByEntityId(id);
            } else {
                throw std::runtime_error("removeByEntityId() not supported for view/lazy storage");
            }
        }
        
        void reserve(size_t capacity) override {
            if constexpr (requires { _storage.reserve(capacity); }) {
                _storage.reserve(capacity);
            }
            // No-op for storage types that don't support reserve
        }
        
        void clear() override {
            if constexpr (requires { _storage.clear(); }) {
                _storage.clear();
            } else {
                throw std::runtime_error("clear() not supported for view/lazy storage");
            }
        }
        
        void setEntityIds(std::vector<EntityId> ids) override {
            if constexpr (requires { _storage.setEntityIds(std::move(ids)); }) {
                _storage.setEntityIds(std::move(ids));
            } else {
                throw std::runtime_error("setEntityIds() not supported for view/lazy storage");
            }
        }
    };

    std::shared_ptr<StorageConcept> _impl;
};

#endif // DIGITAL_INTERVAL_STORAGE_HPP
