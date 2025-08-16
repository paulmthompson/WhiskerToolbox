#include "Line_Data_Binary.hpp"

#include "IO/CapnProto/Serialization.hpp"
#include "Lines/Line_Data.hpp"

#include <capnp/message.h>
#include <capnp/serialize.h>
#include <kj/array.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>


bool save(LineData const & data, BinaryLineSaverOptions & opts) {

    //Check if directory exists
    if (!std::filesystem::exists(opts.parent_dir)) {
        std::filesystem::create_directories(opts.parent_dir);
        std::cout << "Created directory: " << opts.parent_dir << std::endl;
    }

    std::string file_path = opts.parent_dir + "/" + opts.filename;

    try {

        kj::Array<capnp::word> message_words = IO::CapnProto::serializeLineData(&data);
        kj::ArrayPtr<char const> message_chars = message_words.asChars();

        std::ofstream outfile(file_path, std::ios::binary | std::ios::trunc);
        if (!outfile.is_open()) {
            std::cerr << "Error: Could not open file for writing: " << file_path << std::endl;
            return false;
        }

        outfile.write(message_chars.begin(), message_chars.size());

        if (outfile.fail()) {
            std::cerr << "Error: Failed to write all data to file: " << file_path << std::endl;
            outfile.close();
            return false;
        }
        outfile.close();
        return true;

    } catch (kj::Exception const & e) {
        std::cerr << "Cap'n Proto Exception during save: " << e.getDescription().cStr() << std::endl;
        return false;
    } catch (std::exception const & e) {
        std::cerr << "Standard Exception during save: " << e.what() << std::endl;
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

        std::streamsize size = infile.tellg();
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
