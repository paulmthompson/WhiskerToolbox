#ifndef COLOR_HPP
#define COLOR_HPP

#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

inline bool isValidHexColor(const std::string& hex_color) {
    const std::regex hex_color_pattern("^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$");
    return std::regex_match(hex_color, hex_color_pattern);
}

inline bool isValidAlpha(float alpha) {
    return alpha >= 0.0f && alpha <= 1.0f;
}


inline std::string generateRandomColor() {
    std::stringstream ss;
    ss << "#" << std::hex << std::setw(6) << std::setfill('0') << (std::rand() % 0xFFFFFF);
    auto color_string = ss.str();

    std::cout << "Generated color: " << color_string << std::endl;

    return color_string;
}


inline void hexToRGB(const std::string &hexColor, int &r, int &g, int &b) {
    if (!isValidHexColor(hexColor))
    {
        std::cout << "Not valid hex color " << hexColor << std::endl;
        r = 0;
        g = 0;
        b = 0;
        return;
    }

    std::stringstream ss;
    ss << std::hex << hexColor.substr(1, 2);
    ss >> r;
    ss.clear();
    ss << std::hex << hexColor.substr(3, 2);
    ss >> g;
    ss.clear();
    ss << std::hex << hexColor.substr(5, 2);
    ss >> b;
}


#endif // COLOR_HPP
