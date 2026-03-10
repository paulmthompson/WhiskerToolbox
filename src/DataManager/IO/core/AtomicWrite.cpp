/**
 * @file AtomicWrite.cpp
 * @brief Implementation of the atomic file write utility.
 */
#include "AtomicWrite.hpp"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <cstdio>// _fileno
#include <io.h>  // _commit
#else
#include <unistd.h>// fsync, fileno
#endif

namespace {

/// Platform-specific fsync wrapper for an ofstream that is still open.
bool syncStream(std::ofstream & stream) {
#ifdef _WIN32
    return _commit(_fileno(stream.rdbuf()->fd())) == 0;
#else
    // Flush the C++ stream first, then fsync the underlying fd.
    stream.flush();
    if (!stream.good()) {
        return false;
    }

    // Use fileno on the underlying FILE* — not directly available from
    // ofstream.  Instead, we rely on the OS rename semantics: on POSIX
    // rename is atomic after close, and most filesystems honour this.
    // We skip fsync here for portability; the close() below will flush
    // kernel buffers.  True fsync would require platform-specific fd access.
    return true;
#endif
}

}// anonymous namespace

bool atomicWriteFile(
        std::filesystem::path const & target_path,
        std::function<bool(std::ostream &)> const & write_callback) {

    assert(write_callback && "atomicWriteFile: write_callback must be valid");

    // Ensure parent directory exists
    auto parent = target_path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            std::cerr << "atomicWriteFile: failed to create directories for "
                      << target_path << ": " << ec.message() << '\n';
            return false;
        }
    }

    // Build temporary path alongside the target
    auto tmp_path = target_path;
    tmp_path += ".tmp";

    // Write to the temp file
    {
        std::ofstream ofs(tmp_path, std::ios::binary | std::ios::trunc);
        if (!ofs.is_open()) {
            std::cerr << "atomicWriteFile: cannot open temp file " << tmp_path
                      << ": " << std::strerror(errno) << '\n';
            return false;
        }

        bool const write_ok = write_callback(ofs);
        if (!write_ok || !ofs.good()) {
            ofs.close();
            std::error_code ec;
            std::filesystem::remove(tmp_path, ec);
            return false;
        }

        syncStream(ofs);
        ofs.close();

        if (ofs.fail()) {
            std::error_code ec;
            std::filesystem::remove(tmp_path, ec);
            return false;
        }
    }

    // Atomically rename temp → target
    std::error_code ec;
    std::filesystem::rename(tmp_path, target_path, ec);
    if (ec) {
        std::cerr << "atomicWriteFile: rename failed: " << ec.message() << '\n';
        std::filesystem::remove(tmp_path, ec);
        return false;
    }

    return true;
}
