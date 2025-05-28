#ifndef COLOR_HPP
#define COLOR_HPP

#include <string>

bool isValidHexColor(const std::string& hex_color);

bool isValidAlpha(float alpha);

std::string generateRandomColor();

void hexToRGB(const std::string &hexColor, int &r, int &g, int &b);

void hexToRGB(const std::string & hexColor, float & r, float & g, float & b);


#endif // COLOR_HPP
