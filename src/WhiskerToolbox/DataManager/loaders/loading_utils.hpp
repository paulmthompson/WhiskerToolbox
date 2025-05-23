#ifndef LOADING_UTILS_HPP
#define LOADING_UTILS_HPP

#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

template <typename T>
std::optional<std::string> check_dir_and_get_full_path(T opts) {
    if (!opts.parent_dir.empty() && !std::filesystem::exists(opts.parent_dir)) {
        try {
            std::filesystem::create_directories(opts.parent_dir);
            std::cout << "Created directory: " << opts.parent_dir << std::endl;
        } catch (std::filesystem::filesystem_error const& e) {
            std::cerr << "Error creating directory " << opts.parent_dir << ": " << e.what() << std::endl;
            return std::nullopt;
        }
    }

    std::string full_path = opts.parent_dir.empty() ? opts.filename : (std::filesystem::path(opts.parent_dir) / opts.filename).string();
    return full_path;
}
#endif// LOADING_UTILS_HPP
