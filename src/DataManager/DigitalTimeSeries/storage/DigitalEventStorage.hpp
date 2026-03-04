#ifndef DIGITAL_EVENT_STORAGE_HPP
#define DIGITAL_EVENT_STORAGE_HPP

#include "Entity/EntityTypes.hpp"
#include "TimeFrame/TimeFrame.hpp"
#include "DigitalEventStorageCache.hpp"

#include <memory>
#include <optional>
#include <vector>
#include <utility>

/**
 * @brief Hash specialization for TimeFrameIndex to allow use in unordered containers
 */
template<>
struct std::hash<TimeFrameIndex> {
    size_t operator()(TimeFrameIndex const& idx) const noexcept {
        return std::hash<int64_t>{}(idx.getValue());
    }
};

class OwningDigitalEventStorage;
class ViewDigitalEventStorage;
template<typename ViewType>
class LazyDigitalEventStorage;

// =============================================================================
// Type-Erased Storage Wrapper (Virtual Dispatch)
// =============================================================================

/**
 * @brief Type-erased storage wrapper for digital event storage
 * 
 * Provides a uniform interface for any storage backend while hiding the
 * concrete storage type. Supports lazy transforms with unbounded ViewType.
 */
class DigitalEventStorageWrapper {
public:
    /**
     * @brief Construct wrapper from any storage implementation
     */
    template<typename StorageImpl>
    explicit DigitalEventStorageWrapper(StorageImpl storage)
        : _impl(std::make_shared<StorageModel<StorageImpl>>(std::move(storage))) {}

    // Default constructor creates empty owning storage
    DigitalEventStorageWrapper();

    // Copy and move semantics - shared_ptr allows sharing
    DigitalEventStorageWrapper(DigitalEventStorageWrapper &&) noexcept = default;
    DigitalEventStorageWrapper & operator=(DigitalEventStorageWrapper &&) noexcept = default;
    DigitalEventStorageWrapper(DigitalEventStorageWrapper const &) = default;
    DigitalEventStorageWrapper & operator=(DigitalEventStorageWrapper const &) = default;

    // ========== Unified Interface ==========

    [[nodiscard]] size_t size() const { return _impl->size(); }
    [[nodiscard]] bool empty() const { return _impl->size() == 0; }

    [[nodiscard]] TimeFrameIndex getEvent(size_t idx) const {
        return _impl->getEvent(idx);
    }

    [[nodiscard]] EntityId getEntityId(size_t idx) const {
        return _impl->getEntityId(idx);
    }

    [[nodiscard]] std::optional<size_t> findByTime(TimeFrameIndex time) const {
        return _impl->findByTime(time);
    }

    [[nodiscard]] std::optional<size_t> findByEntityId(EntityId id) const {
        return _impl->findByEntityId(id);
    }

    [[nodiscard]] bool hasEventAtTime(TimeFrameIndex time) const {
        return _impl->hasEventAtTime(time);
    }

    [[nodiscard]] std::pair<size_t, size_t> getTimeRange(TimeFrameIndex start, TimeFrameIndex end) const {
        return _impl->getTimeRange(start, end);
    }





    [[nodiscard]] DigitalEventStorageType getStorageType() const {
        return _impl->getStorageType();
    }

    [[nodiscard]] bool isView() const {
        return getStorageType() == DigitalEventStorageType::View;
    }

    [[nodiscard]] bool isLazy() const {
        return getStorageType() == DigitalEventStorageType::Lazy;
    }

    // ========== Cache Optimization ==========

    [[nodiscard]] DigitalEventStorageCache tryGetCache() const {
        return _impl->tryGetCache();
    }

    // ========== Mutation Operations ==========

    bool addEvent(TimeFrameIndex event, EntityId entity_id) {
        return _impl->addEvent(event, entity_id);
    }

    bool removeEvent(TimeFrameIndex event) {
        return _impl->removeEvent(event);
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
    [[nodiscard]] StorageType * tryGet() {
        auto * model = dynamic_cast<StorageModel<StorageType> *>(_impl.get());
        return model ? &model->_storage : nullptr;
    }

    template<typename StorageType>
    [[nodiscard]] StorageType const * tryGet() const {
        auto const * model = dynamic_cast<StorageModel<StorageType> const *>(_impl.get());
        return model ? &model->_storage : nullptr;
    }

    /**
     * @brief Try to get mutable owning storage
     */
    [[nodiscard]] OwningDigitalEventStorage * tryGetMutableOwning();

    [[nodiscard]] OwningDigitalEventStorage const * tryGetOwning() const;

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
    [[nodiscard]] std::shared_ptr<OwningDigitalEventStorage const> getSharedOwningStorage() const;

private:
    struct StorageConcept {
        virtual ~StorageConcept() = default;

        virtual size_t size() const = 0;
        virtual TimeFrameIndex getEvent(size_t idx) const = 0;
        virtual EntityId getEntityId(size_t idx) const = 0;
        virtual std::optional<size_t> findByTime(TimeFrameIndex time) const = 0;
        virtual std::optional<size_t> findByEntityId(EntityId id) const = 0;
        virtual bool hasEventAtTime(TimeFrameIndex time) const = 0;
        virtual std::pair<size_t, size_t> getTimeRange(TimeFrameIndex start, TimeFrameIndex end) const = 0;


        virtual DigitalEventStorageType getStorageType() const = 0;
        virtual DigitalEventStorageCache tryGetCache() const = 0;

        // Mutation
        virtual bool addEvent(TimeFrameIndex event, EntityId entity_id) = 0;
        virtual bool removeEvent(TimeFrameIndex event) = 0;
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

        TimeFrameIndex getEvent(size_t idx) const override {
            return _storage.getEvent(idx);
        }

        EntityId getEntityId(size_t idx) const override {
            return _storage.getEntityId(idx);
        }

        std::optional<size_t> findByTime(TimeFrameIndex time) const override {
            if constexpr (requires { _storage.findByTime(time); }) {
                return _storage.findByTime(time);
            } else {
                return std::nullopt;
            }
        }

        std::optional<size_t> findByEntityId(EntityId id) const override {
            return _storage.findByEntityId(id);
        }

        bool hasEventAtTime(TimeFrameIndex time) const override {
            if constexpr (requires { _storage.hasEventAtTime(time); }) {
                return _storage.hasEventAtTime(time);
            } else {
                return false;
            }
        }

        std::pair<size_t, size_t> getTimeRange(TimeFrameIndex start, TimeFrameIndex end) const override {
            if constexpr (requires { _storage.getTimeRange(start, end); }) {
                return _storage.getTimeRange(start, end);
            } else {
                return {0, 0};
            }
        }





        DigitalEventStorageType getStorageType() const override {
            return _storage.getStorageType();
        }

        DigitalEventStorageCache tryGetCache() const override {
            if constexpr (requires { _storage.tryGetCache(); }) {
                return _storage.tryGetCache();
            } else {
                return DigitalEventStorageCache{};
            }
        }

        // Mutation - only for OwningDigitalEventStorage
        bool addEvent(TimeFrameIndex event, EntityId entity_id) override {
            if constexpr (requires { _storage.addEvent(event, entity_id); }) {
                return _storage.addEvent(event, entity_id);
            } else {
                throw std::runtime_error("addEvent() not supported for view/lazy storage");
            }
        }

        bool removeEvent(TimeFrameIndex event) override {
            if constexpr (requires { _storage.removeEvent(event); }) {
                return _storage.removeEvent(event);
            } else {
                throw std::runtime_error("removeEvent() not supported for view/lazy storage");
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

#endif // DIGITAL_EVENT_STORAGE_HPP
