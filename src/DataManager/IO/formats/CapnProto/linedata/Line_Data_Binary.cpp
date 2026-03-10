#include "Line_Data_Binary.hpp"

#include "IO/core/AtomicWrite.hpp"
#include "Lines/Line_Data.hpp"
#include "common/Serialization.hpp"

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/array.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <vector>


bool save(LineData const & data, BinaryLineSaverOptions const & opts) {

    std::filesystem::path const target_path =
            std::filesystem::path(opts.parent_dir) / opts.filename;

    try {
        kj::Array<capnp::word> message_words = IO::CapnProto::serializeLineData(&data);
        kj::ArrayPtr<char const> message_chars = message_words.asChars();

        return atomicWriteFile(target_path, [&](std::ostream & os) -> bool {
            os.write(message_chars.begin(),
                     static_cast<std::streamsize>(message_chars.size()));
            return !os.fail();
        });

    } catch (kj::Exception const & e) {
        spdlog::error("Cap'n Proto exception during save: {}", e.getDescription().cStr());
        return false;
    } catch (std::exception const & e) {
        spdlog::error("Exception during binary line save: {}", e.what());
        return false;
    }
}

std::shared_ptr<LineData> load(BinaryLineLoaderOptions & opts) {
    try {
        std::ifstream infile(opts.file_path, std::ios::binary | std::ios::ate);
        if (!infile.is_open()) {
            std::cerr << "Error: Could not open file for reading: " << opts.file_path << std::endl;
            return nullptr;
        }

        std::streamsize const size = infile.tellg();
        infile.seekg(0, std::ios::beg);

        if (size == 0) {
            std::cerr << "Error: File is empty: " << opts.file_path << std::endl;
            return nullptr;
        }
        if (size % sizeof(capnp::word) != 0) {
            std::cerr << "Error: File size "
                      << size
                      << " is not a multiple of Cap'n Proto word size ("
                      << sizeof(capnp::word)
                      << ") for file: "
                      << opts.file_path << std::endl;
            return nullptr;
        }

        kj::Array<capnp::word> words = kj::heapArray<capnp::word>(size / sizeof(capnp::word));
        if (!infile.read(reinterpret_cast<char *>(words.begin()), size)) {
            std::cerr << "Error: Failed to read all data from file: " << opts.file_path << std::endl;
            infile.close();
            return nullptr;
        }
        infile.close();

        capnp::ReaderOptions options;
        options.traversalLimitInWords = 256ull * 1024 * 1024;
        return IO::CapnProto::deserializeLineData(words.asPtr(), options);

    } catch (kj::Exception const & e) {
        std::cerr << "Cap'n Proto Exception during load: " << e.getDescription().cStr() << std::endl;
        return nullptr;
    } catch (std::exception const & e) {
        std::cerr << "Standard Exception during load: " << e.what() << std::endl;
        return nullptr;
    }
}

/*
std::shared_ptr<LineData> BinaryFileCapnpStorage::loadMemoryMapped(const void* mapped_data, size_t mapped_size) {
    if (!mapped_data || mapped_size == 0) {
        std::cerr << "Error: Invalid memory mapped region provided." << std::endl;
        return nullptr;
    }
    if (mapped_size % sizeof(capnp::word) != 0) {
        std::cerr << "Error: Mapped size " << mapped_size << " is not a multiple of Cap'n Proto word size (" << sizeof(capnp::word) << ")." << std::endl;
        return nullptr;
    }

}
*/
