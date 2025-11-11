#ifndef RAGGED_TIME_SERIES_HPP
#define RAGGED_TIME_SERIES_HPP

#include "CoreGeometry/ImageSize.hpp"
#include "CoreGeometry/lines.hpp"
#include "CoreGeometry/masks.hpp"
#include "CoreGeometry/points.hpp"
#include "Entity/EntityRegistry.hpp"
#include "Entity/EntityTypes.hpp"
#include "Observer/Observer_Data.hpp"
#include "TimeFrame/TimeFrame.hpp"

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

/**
 * @brief Base template class for ragged time series data structures.
 *
 * RaggedTimeSeries provides a unified interface for time series data where
 * multiple unique entries can exist at each timestamp. Each entry has a unique
 * EntityId for tracking and manipulation.
 *
 * This base class manages:
 * - Time series storage as a map from TimeFrameIndex to vectors of DataEntry<TData>
 * - Image size metadata
 * - TimeFrame association
 * - Identity context (data key and EntityRegistry) for automatic EntityId management
 *
 * Derived classes (LineData, MaskData, PointData) specialize the template parameter
 * TData and provide domain-specific operations.
 *
 * @tparam TData The data type stored in each entry (e.g., Line2D, Mask2D, Point2D<float>)
 */
template <typename TData>
class RaggedTimeSeries : public ObserverData {
public:
    // ========== Constructors ==========
    RaggedTimeSeries() = default;

    virtual ~RaggedTimeSeries() = default;

    // Move constructor
    RaggedTimeSeries(RaggedTimeSeries && other) noexcept
        : ObserverData(std::move(other)),
          _data(std::move(other._data)),
          _image_size(other._image_size),
          _time_frame(std::move(other._time_frame)),
          _identity_data_key(std::move(other._identity_data_key)),
          _identity_registry(other._identity_registry) {
        other._identity_registry = nullptr;
    }

    // Move assignment operator
    RaggedTimeSeries & operator=(RaggedTimeSeries && other) noexcept {
        if (this != &other) {
            ObserverData::operator=(std::move(other));
            _data = std::move(other._data);
            _image_size = other._image_size;
            _time_frame = std::move(other._time_frame);
            _identity_data_key = std::move(other._identity_data_key);
            _identity_registry = other._identity_registry;
            other._identity_registry = nullptr;
        }
        return *this;
    }

    // Delete copy constructor and copy assignment
    RaggedTimeSeries(RaggedTimeSeries const &) = delete;
    RaggedTimeSeries & operator=(RaggedTimeSeries const &) = delete;

    // ========== Time Frame ==========
    /**
     * @brief Set the time frame for this data structure
     * 
     * @param time_frame Shared pointer to the TimeFrame to associate with this data
     */
    void setTimeFrame(std::shared_ptr<TimeFrame> time_frame) { 
        _time_frame = std::move(time_frame); 
    }

    /**
     * @brief Get the current time frame
     * 
     * @return Shared pointer to the associated TimeFrame, or nullptr if none is set
     */
    [[nodiscard]] std::shared_ptr<TimeFrame> getTimeFrame() const {
        return _time_frame;
    }

    // ========== Image Size ==========
    /**
     * @brief Get the image size associated with this data
     * 
     * @return The current ImageSize
     */
    [[nodiscard]] ImageSize getImageSize() const { 
        return _image_size; 
    }

    /**
     * @brief Set the image size for this data
     * 
     * @param image_size The ImageSize to set
     */
    void setImageSize(ImageSize const & image_size) { 
        _image_size = image_size; 
    }

    // ========== Identity Context ==========
    /**
     * @brief Set identity context for automatic EntityId maintenance.
     * 
     * This establishes the connection to an EntityRegistry that will manage
     * EntityIds for all entries in this data structure.
     * 
     * @param data_key String key identifying this data in the EntityRegistry
     * @param registry Pointer to the EntityRegistry (must remain valid)
     * @pre registry != nullptr
     */
    void setIdentityContext(std::string const & data_key, EntityRegistry * registry) {
        _identity_data_key = data_key;
        _identity_registry = registry;
    }

    /**
     * @brief Rebuild EntityIds for all entries using the current identity context.
     * 
     * This method regenerates EntityIds for all data entries across all time frames.
     * If no identity context is set (_identity_registry is nullptr), all EntityIds
     * are reset to EntityId(0).
     * 
     * The EntityKind used depends on the derived class type:
     * - LineData uses EntityKind::LineEntity
     * - MaskData uses EntityKind::MaskEntity
     * - PointData uses EntityKind::PointEntity
     * 
     * @pre Identity context should be set via setIdentityContext before calling
     */
    void rebuildAllEntityIds() {
        if (!_identity_registry) {
            // No registry: reset all EntityIds to 0
            for (auto & [t, entries]: _data) {
                (void) t;  // Suppress unused variable warning
                for (auto & entry: entries) {
                    entry.entity_id = EntityId(0);
                }
            }
            return;
        }

        // Determine the EntityKind based on TData type using if constexpr
        EntityKind kind = getEntityKind();

        // Rebuild EntityIds using the registry
        for (auto & [t, entries]: _data) {
            for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                entries[static_cast<size_t>(i)].entity_id = 
                    _identity_registry->ensureId(_identity_data_key, kind, t, i);
            }
        }
    }

protected:
    /**
     * @brief Get the EntityKind for this data type.
     * 
     * This method uses if constexpr to determine the correct EntityKind
     * based on the TData template parameter at compile time.
     * 
     * @return The EntityKind corresponding to TData
     */
    [[nodiscard]] static constexpr EntityKind getEntityKind() {
        // We need to match on the TData type
        // Forward declarations needed:
        // - Line2D, Mask2D, Point2D<float>
        
        if constexpr (std::is_same_v<TData, Line2D>) {
            return EntityKind::LineEntity;
        } else if constexpr (std::is_same_v<TData, Mask2D>) {
            return EntityKind::MaskEntity;
        } else if constexpr (std::is_same_v<TData, Point2D<float>>) {
            return EntityKind::PointEntity;
        } else {
            // Fallback for unknown types - this will cause a compile error
            // if an unsupported type is used
            static_assert(std::is_same_v<TData, Line2D> || 
                         std::is_same_v<TData, Mask2D> || 
                         std::is_same_v<TData, Point2D<float>>,
                         "TData must be Line2D, Mask2D, or Point2D<float>");
            return EntityKind::LineEntity; // Never reached, but needed for compilation
        }
    }

    // ========== Protected Member Variables ==========
    
    /// Storage for time series data: map from time to vector of entries
    std::map<TimeFrameIndex, std::vector<DataEntry<TData>>> _data;
    
    /// Image size metadata
    ImageSize _image_size;
    
    /// Associated time frame for temporal indexing
    std::shared_ptr<TimeFrame> _time_frame{nullptr};
    
    /// Data key for EntityRegistry lookups
    std::string _identity_data_key;
    
    /// Pointer to EntityRegistry for automatic EntityId management
    EntityRegistry * _identity_registry{nullptr};
};

#endif // RAGGED_TIME_SERIES_HPP
