#ifndef CLONE_MEDIA_FOR_INFERENCE_HPP
#define CLONE_MEDIA_FOR_INFERENCE_HPP

/**
 * @file CloneMediaForInference.hpp
 * @brief Create inference-dedicated MediaData clones (separate decoder/state).
 */

#include <memory>

class MediaData;

/**
 * @brief Clone media for batch inference on a worker thread.
 *
 * Creates a new instance of the same concrete type as @p source, loads the
 * same backing file or directory, and copies the shared TimeFrame when set.
 *
 * @pre source.getFilename() must be non-empty for loadable media types.
 * @return Clone on success, or nullptr if the type is unsupported or load fails.
 */
[[nodiscard]] std::shared_ptr<MediaData> cloneMediaForInference(MediaData const & source);

#endif// CLONE_MEDIA_FOR_INFERENCE_HPP
