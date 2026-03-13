#include "AtomicWrite.hpp"
#include <fstream>
#include <iostream>
#include <system_error>

namespace {

/**
 * For non-mission-critical apps, we ensure the C++ buffer is flushed
 * to the OS. The OS will then handle the physical write during the
 * close() or shortly after.
 */
bool prepareForRename(std::ofstream & stream) {
    stream.flush();
    return stream.good();
}

} // anonymous namespace

bool atomicWriteFile(
        std::filesystem::path const & target_path,
        std::function<bool(std::ostream &)> const & write_callback) {

    if (!write_callback) return false;

    // 1. Ensure parent directory exists
    auto parent = target_path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) return false;
    }

    // 2. Build temporary path (e.g., "data.txt.tmp")
    auto tmp_path = target_path;
    tmp_path += ".tmp";

    // 3. Scope for the ofstream to ensure it closes before renaming
    {
        std::ofstream ofs(tmp_path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) return false;

        // Perform the actual data writing
        if (!write_callback(ofs) || !prepareForRename(ofs)) {
            ofs.close();
            std::error_code ec;
            std::filesystem::remove(tmp_path, ec);
            return false;
        }

        // ofs goes out of scope and closes here
    }

    // 4. Atomically rename temp → target
    std::error_code rename_ec;
    std::filesystem::rename(tmp_path, target_path, rename_ec);

    if (rename_ec) {
        std::error_code remove_ec;
        std::filesystem::remove(tmp_path, remove_ec);
        return false;
    }

    return true;
}
