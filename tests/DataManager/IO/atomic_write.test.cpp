/**
 * @file atomic_write.test.cpp
 * @brief Tests for the atomicWriteFile utility function.
 */
#include <catch2/catch_test_macros.hpp>

#include "IO/core/AtomicWrite.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

/// Read entire file into a string.
std::string readAll(std::filesystem::path const & path) {
    std::ifstream ifs(path, std::ios::binary);
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

/// Create a unique temp directory for a test.
std::filesystem::path makeTempDir() {
    auto dir = std::filesystem::temp_directory_path() / "whisker_atomic_write_test";
    std::filesystem::create_directories(dir);
    return dir;
}

}// anonymous namespace

TEST_CASE("atomicWriteFile success case", "[AtomicWrite]") {
    auto dir = makeTempDir();
    auto target = dir / "output.txt";

    bool ok = atomicWriteFile(target, [](std::ostream & os) {
        os << "hello world";
        return true;
    });

    REQUIRE(ok);
    REQUIRE(std::filesystem::exists(target));
    REQUIRE(readAll(target) == "hello world");

    // Temp file should not linger
    REQUIRE_FALSE(std::filesystem::exists(std::filesystem::path(target.string() + ".tmp")));

    std::filesystem::remove_all(dir);
}

TEST_CASE("atomicWriteFile callback returns false", "[AtomicWrite]") {
    auto dir = makeTempDir();
    auto target = dir / "should_not_exist.txt";

    bool ok = atomicWriteFile(target, [](std::ostream & /*os*/) {
        return false;
    });

    REQUIRE_FALSE(ok);
    REQUIRE_FALSE(std::filesystem::exists(target));
    REQUIRE_FALSE(std::filesystem::exists(std::filesystem::path(target.string() + ".tmp")));

    std::filesystem::remove_all(dir);
}

TEST_CASE("atomicWriteFile creates parent directories", "[AtomicWrite]") {
    auto dir = makeTempDir();
    auto target = dir / "nested" / "deep" / "file.txt";

    bool ok = atomicWriteFile(target, [](std::ostream & os) {
        os << "nested content";
        return true;
    });

    REQUIRE(ok);
    REQUIRE(std::filesystem::exists(target));
    REQUIRE(readAll(target) == "nested content");

    std::filesystem::remove_all(dir);
}

TEST_CASE("atomicWriteFile overwrites existing file atomically", "[AtomicWrite]") {
    auto dir = makeTempDir();
    auto target = dir / "existing.txt";

    // Create an initial file
    {
        std::ofstream ofs(target);
        ofs << "old content";
    }

    bool ok = atomicWriteFile(target, [](std::ostream & os) {
        os << "new content";
        return true;
    });

    REQUIRE(ok);
    REQUIRE(readAll(target) == "new content");

    std::filesystem::remove_all(dir);
}

TEST_CASE("atomicWriteFile preserves original on callback failure", "[AtomicWrite]") {
    auto dir = makeTempDir();
    auto target = dir / "preserved.txt";

    // Create an initial file
    {
        std::ofstream ofs(target);
        ofs << "original";
    }

    bool ok = atomicWriteFile(target, [](std::ostream & os) {
        os << "partial";
        return false;// Simulate failure
    });

    REQUIRE_FALSE(ok);
    REQUIRE(readAll(target) == "original");

    std::filesystem::remove_all(dir);
}

TEST_CASE("atomicWriteFile writes binary data correctly", "[AtomicWrite]") {
    auto dir = makeTempDir();
    auto target = dir / "binary.bin";

    std::string const binary_data = {'\x00', '\x01', '\x02', '\xFF', '\xFE'};

    bool ok = atomicWriteFile(target, [&](std::ostream & os) {
        os.write(binary_data.data(), static_cast<std::streamsize>(binary_data.size()));
        return true;
    });

    REQUIRE(ok);
    REQUIRE(readAll(target) == binary_data);

    std::filesystem::remove_all(dir);
}
