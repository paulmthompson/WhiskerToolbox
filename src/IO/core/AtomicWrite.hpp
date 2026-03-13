/**
 * @file AtomicWrite.hpp
 * @brief Atomic file write utility: writes to a temp file then renames over the target.
 */
#ifndef DATAMANAGER_IO_ATOMIC_WRITE_HPP
#define DATAMANAGER_IO_ATOMIC_WRITE_HPP

#include <filesystem>
#include <functional>
#include <ostream>

/**
 * @brief Write data via callback to a temporary file, then atomically rename over target.
 *
 * The write_callback receives an open std::ostream and should return true on
 * success.  On success the temporary file is fsynced and renamed over
 * @p target_path; on failure the temporary file is removed and @p target_path
 * is left untouched.
 *
 * Parent directories of @p target_path are created if they do not exist.
 *
 * @param target_path  Destination file path.
 * @param write_callback  Callback that writes data to the provided stream.
 *                         Must return true on success, false on error.
 * @return true if the file was written and renamed successfully.
 *
 * @pre write_callback must be a valid callable.
 */
bool atomicWriteFile(
        std::filesystem::path const & target_path,
        std::function<bool(std::ostream &)> const & write_callback);

#endif// DATAMANAGER_IO_ATOMIC_WRITE_HPP
