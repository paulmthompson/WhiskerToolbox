#ifndef MEDIA_OVERRIDES_HPP
#define MEDIA_OVERRIDES_HPP

/**
 * @file MediaOverrides.hpp
 * @brief Type alias for a map of media data overrides.
 *
 * Used by SlotAssembler::runBatchRangeOffline() to provide a separate
 * MediaData instance for each frame in the batch.
 *
 * @see SlotAssembler::runBatchRangeOffline()
 * @see MediaData
 * @see MediaDataFactory
 */


class MediaData;

#include <unordered_map>
#include <string>
#include <memory>

using MediaOverrides = std::unordered_map<std::string, std::shared_ptr<MediaData>>;

#endif