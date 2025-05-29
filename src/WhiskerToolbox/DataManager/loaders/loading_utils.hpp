#ifndef LOADING_UTILS_HPP
#define LOADING_UTILS_HPP

#include "ImageSize/ImageSize.hpp"

#include "nlohmann/json.hpp"

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

template<typename T>
inline void change_image_size_json(T data, nlohmann::basic_json<> const & item) {

    int const height = item.value("height", -1);
    int const width = item.value("width", -1);

    data->setImageSize(ImageSize{.width = width, .height = height});

    int const scale_height = item.value("scaled_height", -1);
    int const scale_width = item.value("scaled_width", -1);

    if ((scale_height == -1)&(scale_width == -1)) {
        return;
    }

    if ((scale_height == height)&(scale_width == width))
    {
        return;
    }

    data->changeImageSize({ .width=scale_width, .height=scale_height});

    return;
}

#endif// LOADING_UTILS_HPP
