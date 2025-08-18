#include "color.hpp"

#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

bool isValidHexColor(const std::string& hex_color) {
    const std::regex hex_color_pattern("^#([A-Fa-f0-9]{6}|[A-Fa-f0-9]{3})$");
    return std::regex_match(hex_color, hex_color_pattern);
}

bool isValidAlpha(float alpha) {
    return alpha >= 0.0f && alpha <= 1.0f;
}


std::string generateRandomColor() {
    std::stringstream ss;
    ss << "#" << std::hex << std::setw(6) << std::setfill('0') << (std::rand() % 0xFFFFFF);
    auto color_string = ss.str();

    std::cout << "Generated color: " << color_string << std::endl;

    return color_string;
}

void hexToRGB(const std::string & hexColor, float & r, float & g, float & b)
{
    int r_int, g_int, b_int;
    
    hexToRGB(hexColor, r_int, g_int, b_int);

    // Convert from 0-255 range to 0-1 range for OpenGL
    r = static_cast<float>(r_int) / 255.0f;
    g = static_cast<float>(g_int) / 255.0f;
    b = static_cast<float>(b_int) / 255.0f;
}

void hexToRGB(const std::string &hexColor, int &r, int &g, int &b) {
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
